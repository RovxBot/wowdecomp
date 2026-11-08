// Actual registered Chorus factory, callbacks, delay processing and pool ownership.
// Failure injection stays in this executable; the production mixer is unchanged.
#include "fmod.hpp"
#include "fmod_dsp_chorus.h"
#include "fmod_memory.h"
#include "fmod_pluginfactory.h"
#include "fmod_systemi.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <typeinfo>
#include <vector>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "chorus check failed at line %d\n", line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)

static int live = 0, failAfter = -1, created = 0, released = 0;
static void* F_CALLBACK allocate(unsigned size, FMOD_MEMORY_TYPE) {
    if (failAfter >= 0 && failAfter-- == 0) return nullptr;
    void* result = std::malloc(size);
    if (result) ++live;
    return result;
}
static void* F_CALLBACK reallocate(void* pointer, unsigned size, FMOD_MEMORY_TYPE) {
    if (!size) {
        if (pointer) --live;
        std::free(pointer);
        return nullptr;
    }
    const bool fresh = !pointer;
    void* result = std::realloc(pointer, size);
    if (result && fresh) ++live;
    return result;
}
static void F_CALLBACK deallocate(void* pointer, FMOD_MEMORY_TYPE) {
    require(pointer && live > 0);
    --live;
    std::free(pointer);
}
static_assert(std::is_trivially_destructible<FMOD::DSPChorus>::value,
              "Reaudit pool lifetime if Chorus acquires destructor-owned resources");

static FMOD::DSPChorus* receiver(FMOD_DSP_STATE* state) {
    auto* dsp = dynamic_cast<FMOD::DSPChorus*>(static_cast<FMOD::DSPI*>(state));
    require(dsp && typeid(*dsp) == typeid(FMOD::DSPChorus));
    require(reinterpret_cast<std::uintptr_t>(dsp) % alignof(FMOD::DSPChorus) == 0);
    return dsp;
}
static FMOD_RESULT F_CALLBACK observeCreate(FMOD_DSP_STATE* state) {
    receiver(state);
    ++created;
    return FMOD::DSPChorus::createCallback(state);
}
static FMOD_RESULT F_CALLBACK observeRelease(FMOD_DSP_STATE* state) {
    receiver(state);
    ++released;
    return FMOD::DSPChorus::releaseCallback(state);
}
// Fail a later default parameter, after the real Chorus callback has allocated
// its delay buffer. The original error must propagate with no leaked state.
static int rejectIndex = 0;
static FMOD_RESULT F_CALLBACK rejectParameter(FMOD_DSP_STATE* state, int index, float value) {
    receiver(state);
    if (index == rejectIndex) return FMOD_ERR_INTERNAL;
    return FMOD::DSPChorus::setParameterCallback(state, index, value);
}

static float parameter(FMOD::DSPI* dsp, int index) {
    float value = -1;
    char text[32] = {};
    require(dsp->getParameter(index, &value, text, sizeof(text)) == FMOD_OK && text[0]);
    return value;
}
static void defaults(FMOD::DSPI* dsp) {
    const float values[] = {0.5f, 0.5f, 0.5f, 0.5f, 40, 0.8f, 0.03f, 0};
    require(dsp->mDescription.numparameters == 8);
    for (int i = 0; i < 8; ++i) require(parameter(dsp, i) == values[i]);
}
static void parameters(FMOD::DSPI* dsp) {
    require(dsp->setParameter(-1, 1) == FMOD_ERR_INVALID_PARAM);
    require(dsp->setParameter(9, 1) == FMOD_ERR_INVALID_PARAM);
    require(dsp->getParameter(9, nullptr, nullptr, 0) == FMOD_ERR_INVALID_PARAM);
    for (int i = 0; i < 8; ++i) {
        const auto& desc = dsp->mDescription.paramdesc[i];
        for (float value : {desc.min - 1, desc.min, desc.max, desc.max + 1, desc.defaultval}) {
            require(dsp->setParameter(i, value) == FMOD_OK);
            require(parameter(dsp, i) == std::clamp(value, desc.min, desc.max));
        }
    }
    defaults(dsp);
}

// With rate/depth zero each tap is an ordinary delay, giving an independent
// sample reference for original dry/wet gains, int16 quantization and feedback.
static void delayProcessing(FMOD::DSPI* dsp, int rate, int channels, float delay,
                            int tap, float feedback) {
    require(dsp->setParameter(0, 0.25f) == FMOD_OK);
    for (int i = 1; i <= 3; ++i)
        require(dsp->setParameter(i, tap == 0 || tap == i ? 0.5f : 0) == FMOD_OK);
    require(dsp->setParameter(4, delay) == FMOD_OK);
    require(dsp->setParameter(5, 0) == FMOD_OK);
    require(dsp->setParameter(6, 0) == FMOD_OK);
    require(dsp->setParameter(7, feedback) == FMOD_OK);
    require(dsp->reset() == FMOD_OK);
    const int ring = std::max(4, int(float(rate) * delay / 1000) * 2);
    const int frames = 3 * ring + 257;
    std::vector<short> history(ring * channels);
    std::vector<float> input(frames * channels), output(input.size()), expected(input.size());
    for (int i = 0; i < frames; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            const int offset = i * channels + ch;
            input[offset] = ch == 1 ? 0 : float(0.125 * std::sin(i * 0.1) / (ch + 1));
            float value = input[offset] * 0.25f * 32768.0f;
            const float wet = history[((i + ring / 2) % ring) * channels + ch];
            for (int t = 1; t <= 3; ++t)
                value += wet * (tap == 0 || tap == t ? 0.5f : 0);
            const float stored = input[offset] * 32767.0f + value * feedback;
            history[(i % ring) * channels + ch] = short(std::clamp(stored, -32768.0f, 32767.0f));
            expected[offset] = value / 32767.0f;
        }
    }
    for (int i = 0; i < frames; i += 127) {
        const int count = std::min(127, frames - i);
        require(dsp->mDescription.read(dsp, input.data() + i * channels,
                output.data() + i * channels, count, channels, channels) == FMOD_OK);
    }
    float maximum = 0;
    for (std::size_t i = 0; i < output.size(); ++i) {
        require(std::isfinite(output[i]));
        maximum = std::max(maximum, std::fabs(output[i] - expected[i]));
    }
    // Each truncation contributes less than one int16 unit. Bound the two
    // quantized recurrences by their feedback contraction, then the wet gain.
    const float wetGain = tap == 0 ? 1.5f : 0.5f;
    const float bound = (2 * wetGain / (1 - feedback * wetGain)) / 32767.0f + 0.000002f;
    std::printf("sine-reference: rate=%d channels=%d delay=%g tap=%d feedback=%g maximum=%.9g quantization-bound=%.9g\n",
                rate, channels, delay, tap, feedback, maximum, bound);
    require(maximum < bound);
    // In-place processing preserves the dry-only original gain.
    require(dsp->setParameter(0, 1) == FMOD_OK);
    for (int i = 1; i <= 3; ++i) require(dsp->setParameter(i, 0) == FMOD_OK);
    output = input;
    require(dsp->mDescription.read(dsp, output.data(), output.data(), frames,
                                  channels, channels) == FMOD_OK);
    for (std::size_t i = 0; i < output.size(); ++i)
        require(std::fabs(output[i] - input[i] * 32768.0f / 32767.0f) < 0.000001f);
    require(dsp->reset() == FMOD_OK);
    std::fill(input.begin(), input.end(), 0);
    require(dsp->mDescription.read(dsp, input.data(), output.data(), frames,
                                  channels, channels) == FMOD_OK);
    for (float sample : output) require(sample == 0);
}

// Exercise the actual three modulated taps, default initialization, wraparound
// and channel independence. Block partitioning must not change the signal.
static void modulation(FMOD::SystemI* internal, int rate, int channels) {
    FMOD::DSPI *whole = nullptr, *split = nullptr;
    require(internal->createDSPByType(FMOD_DSP_TYPE_CHORUS, &whole) == FMOD_OK);
    require(internal->createDSPByType(FMOD_DSP_TYPE_CHORUS, &split) == FMOD_OK);
    defaults(whole);
    defaults(split);
    const int frames = rate;
    std::vector<float> input(frames * channels), first(input.size()), second(input.size());
    for (int i = 0; i < frames; ++i) input[i * channels] = float(0.1 * std::sin(i * 0.05));
    require(whole->mDescription.read(whole, input.data(), first.data(), frames,
                                   channels, channels) == FMOD_OK);
    for (int i = 0; i < frames; i += 113)
        require(split->mDescription.read(split, input.data() + i * channels,
                second.data() + i * channels, std::min(113, frames - i),
                channels, channels) == FMOD_OK);
    double wetEnergy = 0;
    for (int i = 0; i < frames; ++i) {
        const int offset = i * channels;
        require(std::isfinite(first[offset]) && std::fabs(first[offset]) < 1);
        const float wet = first[offset] - input[offset] * 0.5f * 32768.0f / 32767.0f;
        wetEnergy += wet * wet;
        for (int ch = 1; ch < channels; ++ch) require(first[offset + ch] == 0);
    }
    require(wetEnergy > 1);
    require(first == second);
    require(whole->release() == FMOD_OK && split->release() == FMOD_OK);
}

int main() {
    static_assert(alignof(FMOD::DSPChorus) <= alignof(std::max_align_t),
                  "Reaudit the FMOD pool if Chorus requires extended alignment");
    std::printf("chorus-layout: size=%zu alignment=%zu\n", sizeof(FMOD::DSPChorus), alignof(FMOD::DSPChorus));
    require(FMOD_Memory_Initialize(nullptr, 0, allocate, reallocate, deallocate) == FMOD_OK);
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    FMOD::SystemI* internal = nullptr;
    require(FMOD::SystemI::validate(system, &internal) == FMOD_OK && internal);
    require(internal->setUpPlugins() == FMOD_OK);
    // Real pthread-backed connection services normally initialized by SystemI::init.
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(!*slot && FMOD_OS_CriticalSection_Create(slot) == FMOD_OK);
    }
    internal->mConnectionRequestUsedHead.initNode();
    internal->mConnectionRequestFreeHead.initNode();
    for (auto& request : internal->mConnectionRequest) {
        request.initNode();
        request.addBefore(&internal->mConnectionRequestFreeHead);
    }
    auto* factory = internal->mPluginFactory;
    FMOD::FMOD_DSP_DESCRIPTION_EX* desc = nullptr;
    int count = 0;
    require(factory->getNumDSPs(&count) == FMOD_OK);
    for (int i = 0; i < count; ++i) {
        FMOD::FMOD_DSP_DESCRIPTION_EX* candidate = nullptr;
        require(factory->getDSP(i, &candidate) == FMOD_OK);
        if (candidate->mType == FMOD_DSP_TYPE_CHORUS) desc = candidate;
    }
    require(desc && desc->mCategory == FMOD::FMOD_DSP_CATEGORY_FILTER);
    require(desc->mSize == sizeof(FMOD::DSPChorus));
    require(desc->create == FMOD::DSPChorus::createCallback);
    require(desc->release == FMOD::DSPChorus::releaseCallback);
    require(desc->reset == FMOD::DSPChorus::resetCallback);
    require(desc->read == FMOD::DSPChorus::readCallback);
    require(desc->setparameter == FMOD::DSPChorus::setParameterCallback);
    require(desc->getparameter == FMOD::DSPChorus::getParameterCallback);
    const int baseline = live;
    FMOD::DSPI* dsp = nullptr;
    require(factory->createDSP(nullptr, &dsp) == FMOD_ERR_INVALID_PARAM);
    require(factory->createDSP(desc, nullptr) == FMOD_ERR_INVALID_PARAM);
    for (int allocation : {0, 1}) {
        failAfter = allocation;
        require(internal->createDSPByType(FMOD_DSP_TYPE_CHORUS, &dsp) == FMOD_ERR_MEMORY);
        require(!dsp && failAfter == -1 && live == baseline);
    }
    for (int rate : {22050, 48000}) {
        for (int channels : {1, 2, 6}) {
            require(system->setSoftwareFormat(rate, FMOD_SOUND_FORMAT_PCMFLOAT, channels, 0,
                                             FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
            modulation(internal, rate, channels);
            require(live == baseline);
            require(internal->createDSPByType(FMOD_DSP_TYPE_CHORUS, &dsp) == FMOD_OK);
            receiver(dsp);
            require(dsp->instance == (FMOD_DSP*)dsp && dsp->mSystem == internal);
            require(live == baseline + 2); // Object and delay buffer.
            defaults(dsp);
            parameters(dsp);
            for (float delay : {0.0f, 1.0f, 40.0f, 100.0f})
                for (int tap : {0, 1, 2, 3})
                    delayProcessing(dsp, rate, channels, delay, tap, 0.5f);
            require(receiver(dsp)->startBuffering(64) == FMOD_OK);
            require(dsp->release() == FMOD_OK && live == baseline);
            dsp = nullptr;
        }
    }
    // Both factory-owned and caller-preconstructed paths retain error ownership.
    for (bool preconstructed : {false, true}) {
        for (int mode = 0; mode < 11; ++mode) {
            auto bad = *desc;
            bad.create = observeCreate;
            bad.release = observeRelease;
            bad.channels = mode == 0 ? -1 : mode == 1 ? 100 : 0;
            if (mode >= 3) {
                rejectIndex = mode - 3;
                bad.setparameter = rejectParameter;
            }
            dsp = preconstructed ? FMOD_Object_Calloc(FMOD::DSPChorus) : nullptr;
            if (preconstructed) require(dsp);
            if (mode == 2) failAfter = preconstructed ? 0 : 1;
            const auto error = mode == 0 ? FMOD_ERR_INVALID_PARAM :
                               mode == 1 ? FMOD_ERR_TOOMANYCHANNELS :
                               mode == 2 ? FMOD_ERR_MEMORY : FMOD_ERR_INTERNAL;
            require(factory->createDSP(&bad, &dsp) == error);
            require(live == baseline && released == 0 && failAfter == -1);
            dsp = nullptr; // Original factory frees even preconstructed storage on error.
        }
    }
    require(created == 18);
    // The concrete minimum extent is independent of a copied description's size.
    auto observed = *desc;
    observed.mSize = 0;
    observed.create = observeCreate;
    observed.release = observeRelease;
    require(factory->createDSP(&observed, &dsp) == FMOD_OK && live == baseline + 2);
    require(dsp->release() == FMOD_OK && live == baseline && released == 1);
    dsp = FMOD_Object_Calloc(FMOD::DSPChorus);
    require(dsp && factory->createDSP(&observed, &dsp) == FMOD_OK);
    require(receiver(dsp)->startBuffering(64) == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && live == baseline + 1 && released == 2);
    receiver(dsp); // release(false) retains the concrete object lifetime.
    require(dsp->reset() == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && live == baseline + 1 && released == 3);
    for (int i = 0; i < 3; ++i) {
        require(observed.create(dsp) == FMOD_OK && live == baseline + 2);
        defaults(dsp);
        require(dsp->release(false) == FMOD_OK && live == baseline + 1);
    }
    require(dsp->release() == FMOD_OK && released == 7 && live == baseline);
    require(internal->mConnectionRequestUsedHead.isEmpty());
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(FMOD_OS_CriticalSection_Free(*slot) == FMOD_OK);
        *slot = nullptr;
    }
    require(system->release() == FMOD_OK && live == 0);
    require(FMOD_Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr) == FMOD_OK);
    std::puts("native-chorus=passed: registered type/callbacks, defaults/parameters, dry/wet/feedback/delay/modulation/channel/block/in-place/reset processing, allocation/partial-create failures, history/release(false)/reinitialization and pool balance");
}
