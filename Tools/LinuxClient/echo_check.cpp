// Actual registered Echo factory, callbacks, delay processing and pool ownership.
// Failure injection stays in this executable; the production mixer is unchanged.
#include "fmod.hpp"
#include "fmod_dsp_echo.h"
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
        std::fprintf(stderr, "echo check failed at line %d\n", line);
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
static_assert(std::is_trivially_destructible<FMOD::DSPEcho>::value,
              "Reaudit pool lifetime if Echo acquires destructor-owned resources");

static FMOD::DSPEcho* receiver(FMOD_DSP_STATE* state) {
    auto* dsp = dynamic_cast<FMOD::DSPEcho*>(static_cast<FMOD::DSPI*>(state));
    require(dsp && typeid(*dsp) == typeid(FMOD::DSPEcho));
    require(reinterpret_cast<std::uintptr_t>(dsp) % alignof(FMOD::DSPEcho) == 0);
    return dsp;
}
static FMOD_RESULT F_CALLBACK observeCreate(FMOD_DSP_STATE* state) {
    receiver(state);
    ++created;
    return FMOD::DSPEcho::createCallback(state);
}
static FMOD_RESULT F_CALLBACK observeRelease(FMOD_DSP_STATE* state) {
    receiver(state);
    ++released;
    return FMOD::DSPEcho::releaseCallback(state);
}
// Fail a later default parameter, after the real Echo callback has allocated
// its delay buffer. The original error must propagate with no leaked state.
static int rejectIndex = 0;
static FMOD_RESULT F_CALLBACK rejectParameter(FMOD_DSP_STATE* state, int index, float value) {
    receiver(state);
    if (index == rejectIndex) return FMOD_ERR_INTERNAL;
    return FMOD::DSPEcho::setParameterCallback(state, index, value);
}

static float parameter(FMOD::DSPI* dsp, int index) {
    float value = -1;
    char text[32] = {};
    require(dsp->getParameter(index, &value, text, sizeof(text)) == FMOD_OK && text[0]);
    return value;
}
static void defaults(FMOD::DSPI* dsp) {
    const float values[] = {500, 0.5f, 0, 1, 1};
    require(dsp->mDescription.numparameters == 5);
    for (int i = 0; i < 5; ++i) require(parameter(dsp, i) == values[i]);
}
static void parameters(FMOD::DSPI* dsp) {
    // The shared one-past-count guard is tracked separately in #202.
    require(dsp->setParameter(-1, 1) == FMOD_ERR_INVALID_PARAM);
    require(dsp->setParameter(6, 1) == FMOD_ERR_INVALID_PARAM);
    require(dsp->getParameter(6, nullptr, nullptr, 0) == FMOD_ERR_INVALID_PARAM);
    for (int i = 0; i < 5; ++i) {
        const auto& desc = dsp->mDescription.paramdesc[i];
        for (float value : {desc.min - 1, desc.min, desc.max, desc.max + 1, desc.defaultval}) {
            require(dsp->setParameter(i, value) == FMOD_OK);
            float expected = std::clamp(value, desc.min, desc.max);
            if (i == FMOD_DSP_ECHO_MAXCHANNELS) expected = float(int(expected));
            require(parameter(dsp, i) == expected);
        }
    }
    // Max-channels truncation and the original channel-limit bypass.
    for (float value : {0.5f, 1.5f, 16.5f, 0.0f}) {
        require(dsp->setParameter(2, value) == FMOD_OK);
        require(parameter(dsp, 2) == float(int(std::clamp(value, 0.0f, 16.0f))));
    }
    require(dsp->setParameter(2, 1) == FMOD_OK);
    float input[] = {0.25f, -0.5f, 1, 0}, output[4] = {};
    require(dsp->mDescription.read(dsp, input, output, 2, 2, 2) == FMOD_OK);
    require(!std::memcmp(input, output, sizeof(input)));
    require(dsp->setParameter(2, 0) == FMOD_OK);
    defaults(dsp);
}

// Independent frame-by-frame feedback delay, covering each specialized read
// path. Exact dyadic inputs/gains avoid an ambiguous floating-point oracle.
static void processing(FMOD::DSPI* dsp, int rate, int channels, float delay,
                       float decay, float dry, float wet, bool configure = true) {
    if (configure) {
        require(dsp->setParameter(0, delay) == FMOD_OK);
        require(dsp->setParameter(1, decay) == FMOD_OK);
        require(dsp->setParameter(3, dry) == FMOD_OK);
        require(dsp->setParameter(4, wet) == FMOD_OK);
        require(dsp->reset() == FMOD_OK);
    }
    // Preserve the original cast-before-integer-division delay rounding.
    const int ring = int(float(rate) * delay) / 1000;
    require(ring > 0);
    const int frames = 2 * ring + 257;
    std::vector<float> history(ring * channels), input(frames * channels);
    std::vector<float> expected(input.size()), output(input.size());
    for (int i = 0; i < frames; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            const int offset = i * channels + ch;
            input[offset] = ch == 1 ? 0 : float((i * 37) % 257 - 128) / (2048.0f * (1 << ch));
            float& old = history[(i % ring) * channels + ch];
            expected[offset] = input[offset] * dry + old * wet;
            old = input[offset] + old * decay;
        }
    }
    for (int i = 0; i < frames; i += 113)
        require(dsp->mDescription.read(dsp, input.data() + i * channels,
                output.data() + i * channels, std::min(113, frames - i),
                channels, channels) == FMOD_OK);
    for (std::size_t i = 0; i < output.size(); ++i)
        require(std::isfinite(output[i]) && std::fabs(output[i] - expected[i]) < 0.000002f);
    // Whole-buffer in-place operation must equal the same partitioned signal.
    require(dsp->reset() == FMOD_OK);
    output = input;
    require(dsp->mDescription.read(dsp, output.data(), output.data(), frames,
                                  channels, channels) == FMOD_OK);
    for (std::size_t i = 0; i < output.size(); ++i)
        require(std::fabs(output[i] - expected[i]) < 0.000002f);
    require(dsp->reset() == FMOD_OK);
    std::fill(input.begin(), input.end(), 0);
    require(dsp->mDescription.read(dsp, input.data(), output.data(), frames,
                                  channels, channels) == FMOD_OK);
    for (float sample : output) require(sample == 0);
}

int main() {
    static_assert(alignof(FMOD::DSPEcho) <= alignof(std::max_align_t),
                  "Reaudit the FMOD pool if Echo requires extended alignment");
    std::printf("echo-layout: size=%zu alignment=%zu\n", sizeof(FMOD::DSPEcho), alignof(FMOD::DSPEcho));
    require(FMOD_Memory_Initialize(nullptr, 0, allocate, reallocate, deallocate) == FMOD_OK);
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    FMOD::SystemI* internal = nullptr;
    require(FMOD::SystemI::validate(system, &internal) == FMOD_OK && internal);
    require(internal->setUpPlugins() == FMOD_OK);
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
        if (candidate->mType == FMOD_DSP_TYPE_ECHO) desc = candidate;
    }
    require(desc && desc->mCategory == FMOD::FMOD_DSP_CATEGORY_FILTER);
    require(desc->mSize == sizeof(FMOD::DSPEcho));
    require(desc->create == FMOD::DSPEcho::createCallback);
    require(desc->release == FMOD::DSPEcho::releaseCallback);
    require(desc->reset == FMOD::DSPEcho::resetCallback);
    require(desc->read == FMOD::DSPEcho::readCallback);
    require(desc->setparameter == FMOD::DSPEcho::setParameterCallback);
    require(desc->getparameter == FMOD::DSPEcho::getParameterCallback);
    const int baseline = live;
    FMOD::DSPI* dsp = nullptr;
    require(factory->createDSP(nullptr, &dsp) == FMOD_ERR_INVALID_PARAM);
    require(factory->createDSP(desc, nullptr) == FMOD_ERR_INVALID_PARAM);
    for (int allocation : {0, 1}) {
        failAfter = allocation;
        require(internal->createDSPByType(FMOD_DSP_TYPE_ECHO, &dsp) == FMOD_ERR_MEMORY);
        require(!dsp && failAfter == -1 && live == baseline);
    }
    for (int rate : {22050, 48000}) {
        for (int channels : {1, 2, 6}) {
            require(system->setSoftwareFormat(rate, FMOD_SOUND_FORMAT_PCMFLOAT, channels, 0,
                                             FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
            require(internal->createDSPByType(FMOD_DSP_TYPE_ECHO, &dsp) == FMOD_OK);
            receiver(dsp);
            require(dsp->instance == (FMOD_DSP*)dsp && dsp->mSystem == internal);
            require(live == baseline + 2);
            defaults(dsp);
            processing(dsp, rate, channels, 500, 0.5f, 1, 1, false);
            parameters(dsp);
            for (float delay : {1.0f, 1.75f, 40.0f, 5000.0f})
                for (float decay : {0.0f, 0.5f, 1.0f})
                    processing(dsp, rate, channels, delay, decay, 0.5f, 0.25f);
            require(receiver(dsp)->startBuffering(64) == FMOD_OK);
            require(dsp->release() == FMOD_OK && live == baseline);
            dsp = nullptr;
        }
    }
    for (bool preconstructed : {false, true}) {
        for (int mode = 0; mode < 8; ++mode) {
            auto bad = *desc;
            bad.create = observeCreate;
            bad.release = observeRelease;
            bad.channels = mode == 0 ? -1 : mode == 1 ? 100 : 0;
            if (mode >= 3) {
                rejectIndex = mode - 3;
                bad.setparameter = rejectParameter;
            }
            dsp = preconstructed ? FMOD_Object_Calloc(FMOD::DSPEcho) : nullptr;
            if (preconstructed) require(dsp);
            if (mode == 2) failAfter = preconstructed ? 0 : 1;
            const auto error = mode == 0 ? FMOD_ERR_INVALID_PARAM :
                               mode == 1 ? FMOD_ERR_TOOMANYCHANNELS :
                               mode == 2 ? FMOD_ERR_MEMORY : FMOD_ERR_INTERNAL;
            require(factory->createDSP(&bad, &dsp) == error);
            require(live == baseline && released == 0 && failAfter == -1);
            dsp = nullptr;
        }
        // A channel default can replace the already allocated delay buffer.
        auto bad = *desc;
        std::vector<FMOD_DSP_PARAMETERDESC> params(bad.paramdesc, bad.paramdesc + 5);
        params[2].defaultval = 1;
        bad.paramdesc = params.data();
        bad.create = observeCreate;
        bad.release = observeRelease;
        dsp = preconstructed ? FMOD_Object_Calloc(FMOD::DSPEcho) : nullptr;
        failAfter = preconstructed ? 1 : 2;
        require(factory->createDSP(&bad, &dsp) == FMOD_ERR_MEMORY);
        require(failAfter == -1 && live == baseline && released == 0);
        dsp = nullptr;
    }
    require(created == 14);
    auto observed = *desc;
    observed.mSize = 0;
    observed.create = observeCreate;
    observed.release = observeRelease;
    require(factory->createDSP(&observed, &dsp) == FMOD_OK && live == baseline + 2);
    require(dsp->release() == FMOD_OK && live == baseline && released == 1);
    dsp = FMOD_Object_Calloc(FMOD::DSPEcho);
    require(dsp && factory->createDSP(&observed, &dsp) == FMOD_OK);
    require(receiver(dsp)->startBuffering(64) == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && live == baseline + 1 && released == 2);
    receiver(dsp);
    require(dsp->reset() == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && live == baseline + 1 && released == 3);
    // Replacing a buffer on a live object can fail; null-buffer bypass and
    // subsequent recovery must retain their original behavior and ownership.
    require(observed.create(dsp) == FMOD_OK && live == baseline + 2);
    failAfter = 0;
    require(dsp->setParameter(0, 40) == FMOD_ERR_MEMORY && failAfter == -1);
    require(live == baseline + 1);
    float input[] = {0.25f, -0.5f, 1, 0}, output[4] = {};
    require(dsp->mDescription.read(dsp, input, output, 2, 2, 2) == FMOD_OK);
    require(!std::memcmp(input, output, sizeof(input)));
    require(dsp->reset() == FMOD_OK);
    require(dsp->setParameter(0, 40) == FMOD_OK && live == baseline + 2);
    require(dsp->release(false) == FMOD_OK && live == baseline + 1 && released == 4);
    for (int i = 0; i < 3; ++i) {
        require(observed.create(dsp) == FMOD_OK && live == baseline + 2);
        defaults(dsp);
        require(dsp->release(false) == FMOD_OK && live == baseline + 1);
    }
    require(dsp->release() == FMOD_OK && released == 8 && live == baseline);
    require(internal->mConnectionRequestUsedHead.isEmpty());
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(FMOD_OS_CriticalSection_Free(*slot) == FMOD_OK);
        *slot = nullptr;
    }
    require(system->release() == FMOD_OK && live == 0);
    require(FMOD_Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr) == FMOD_OK);
    std::puts("native-echo=passed: registered type/alignment/callbacks, defaults/clamps/rounding, mono/stereo/multichannel delay/decay/mix, partition/in-place/reset, allocation/default/replacement failures, bypass/recovery, history/release(false)/reinitialization and pool balance");
}
