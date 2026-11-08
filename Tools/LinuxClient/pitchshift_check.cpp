// Actual registered pitch-shift factory, callbacks, processing and pool ownership.
// Failure injection is confined to this executable; no replacement DSP or mixer.
#include "fmod.hpp"
#include "fmod_dsp_pitchshift.h"
#include "fmod_dsp_lowpass_simple.h"
#include "fmod_dsp_chorus.h"
#include "fmod_dsp_echo.h"
#include "fmod_dsp_lowpass.h"
#include "fmod_dsp_lowpass2.h"
#include "fmod_dsp_highpass.h"
#include "fmod_dsp_parameq.h"
#include "fmod_memory.h"
#include "fmod_pluginfactory.h"
#include "fmod_systemi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <typeinfo>
#include <vector>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "pitchshift check failed at line %d\n", line);
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
static_assert(std::is_trivially_destructible<FMOD::DSPPitchShift>::value,
              "Reaudit pool lifetime if pitch-shift acquires destructor-owned resources");

static FMOD::DSPPitchShift* receiver(FMOD_DSP_STATE* state) {
    auto* dsp = dynamic_cast<FMOD::DSPPitchShift*>(static_cast<FMOD::DSPI*>(state));
    require(dsp && typeid(*dsp) == typeid(FMOD::DSPPitchShift));
    return dsp;
}
static FMOD_RESULT F_CALLBACK observeCreate(FMOD_DSP_STATE* state) {
    receiver(state);
    ++created;
    return FMOD::DSPPitchShift::createCallback(state);
}
static FMOD_RESULT F_CALLBACK observeRelease(FMOD_DSP_STATE* state) {
    receiver(state);
    ++released;
    return FMOD::DSPPitchShift::releaseCallback(state);
}
// Fail a later default parameter, after the real pitch callback has allocated
// its per-channel state. The original error must propagate with no leaked state.
static FMOD_RESULT F_CALLBACK rejectParameter(FMOD_DSP_STATE* state, int index, float value) {
    receiver(state);
    if (index == FMOD_DSP_PITCHSHIFT_FFTSIZE) return FMOD_ERR_INTERNAL;
    return FMOD::DSPPitchShift::setParameterCallback(state, index, value);
}
static float parameter(FMOD::DSPI* dsp, int index) {
    float value = -1;
    char text[32] = {};
    require(dsp->getParameter(index, &value, text, sizeof(text)) == FMOD_OK && text[0]);
    return value;
}
static void defaults(FMOD::DSPI* dsp) {
    require(parameter(dsp, 0) == 1 && parameter(dsp, 1) == 1024);
    require(parameter(dsp, 2) == 4 && parameter(dsp, 3) == 0);
}
static void parameters(FMOD::DSPI* dsp) {
    require(dsp->setParameter(-1, 1) == FMOD_ERR_INVALID_PARAM);
    require(dsp->setParameter(5, 1) == FMOD_ERR_INVALID_PARAM);
    for (float pitch : {-1.0f, 0.5f, 1.0f, 2.0f, 100.0f}) {
        require(dsp->setParameter(0, pitch) == FMOD_OK);
        require(parameter(dsp, 0) == std::clamp(pitch, 0.5f, 2.0f));
    }
    // Preserve original power-of-two rounding and fixed overlap behavior.
    for (auto pair : {std::pair<float, float>{0, 256}, {256, 256}, {257, 512},
                      {513, 1024}, {1025, 2048}, {2049, 4096}, {5000, 4096}}) {
        require(dsp->setParameter(1, pair.first) == FMOD_OK);
        require(parameter(dsp, 1) == pair.second);
    }
    for (float overlap : {0.0f, 1.0f, 32.0f, 99.0f}) {
        require(dsp->setParameter(2, overlap) == FMOD_OK && parameter(dsp, 2) == 4);
    }
    const int baseline = live;
    for (float channels : {-1.0f, 1.0f, 2.0f, 16.0f, 99.0f, 0.0f}) {
        require(dsp->setParameter(3, channels) == FMOD_OK);
        require(parameter(dsp, 3) == std::clamp(channels, 0.0f, 16.0f));
        require(live == baseline); // Old channel state replaced, not accumulated.
    }
    require(dsp->setParameter(0, 1) == FMOD_OK);
    require(dsp->setParameter(1, 1024) == FMOD_OK);
    defaults(dsp);
}
static void processing(FMOD::DSPI* dsp, int rate, int channels, int fft, float pitch) {
    require(dsp->setParameter(3, float(channels)) == FMOD_OK);
    require(dsp->setParameter(1, float(fft)) == FMOD_OK);
    require(dsp->setParameter(0, pitch) == FMOD_OK);
    require(dsp->reset() == FMOD_OK);
    constexpr int frames = 32768, block = 256;
    std::vector<float> input(frames * channels), output(input.size());
    const double frequency = rate / 64.0;
    for (int i = 0; i < frames; ++i)
        for (int ch = 0; ch < channels; ++ch)
            input[i * channels + ch] = float(0.25 * std::sin(2 * 3.141592653589793 * frequency * i / rate) / (ch + 1));
    const auto read = dsp->mDescription.read;
    for (int i = 0; i < frames; i += block)
        require(read(dsp, input.data() + i * channels, output.data() + i * channels,
                     block, channels, channels) == FMOD_OK);
    double energy = 0;
    int crossings = 0;
    for (int i = frames / 2; i < frames; ++i) {
        const float sample = output[i * channels];
        require(std::isfinite(sample) && std::fabs(sample) < 2.0f);
        energy += sample * sample;
        if (sample > 0 && output[(i - 1) * channels] <= 0) ++crossings;
        for (int ch = 1; ch < channels; ++ch)
            require(std::fabs(output[i * channels + ch] * (ch + 1) - sample) < 0.002f);
    }
    const double measured = double(crossings) * rate / (frames / 2);
    std::printf("pitchshift: rate=%d channels=%d fft=%d pitch=%.1f frequency=%.3f expected=%.3f energy=%.3f\n",
                rate, channels, fft, pitch, measured, frequency * pitch, energy);
    require(energy > 0.01);
    require(std::fabs(measured - frequency * pitch) < frequency * pitch * 0.05 + 4);
    // Reset must clear FIFO/overlap state. Also exercise in-place processing.
    require(dsp->reset() == FMOD_OK);
    std::fill(output.begin(), output.end(), 0);
    require(read(dsp, output.data(), output.data(), block, channels, channels) == FMOD_OK);
    for (float sample : output) require(sample == 0);
}

int main() {
    require(FMOD_Memory_Initialize(nullptr, 0, allocate, reallocate, deallocate) == FMOD_OK);
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    FMOD::SystemI* internal = nullptr;
    require(FMOD::SystemI::validate(system, &internal) == FMOD_OK && internal);
    require(internal->setUpPlugins() == FMOD_OK && internal->mPluginFactory);
    // The isolated DSP fixture needs the real connection locks/queue normally
    // installed by SystemI::init before any DSP release. Use the production
    // pthread-backed services and embedded request nodes, without a fake mixer.
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
    // Audit construction without executing mismatched callbacks: this retains
    // evidence for follow-up without introducing unrelated implementation changes.
    for (int i = 0; i < count; ++i) {
        FMOD::FMOD_DSP_DESCRIPTION_EX* candidate = nullptr;
        require(factory->getDSP(i, &candidate) == FMOD_OK);
        if (candidate->mType == FMOD_DSP_TYPE_PITCHSHIFT) desc = candidate;
        if (candidate->mCategory != FMOD::FMOD_DSP_CATEGORY_FILTER) continue;
        auto audit = *candidate;
        audit.create = nullptr;
        audit.release = nullptr;
        FMOD::DSPI* instance = nullptr;
        require(factory->createDSP(&audit, &instance) == FMOD_OK);
        std::printf("filter-inventory: type=%d category=%d name=%s size=%u construction=%s\n",
                    candidate->mType, candidate->mCategory, candidate->name,
                    candidate->mSize, typeid(*instance).name());
        if (candidate->mType == FMOD_DSP_TYPE_PITCHSHIFT)
            require(typeid(*instance) == typeid(FMOD::DSPPitchShift));
        else if (candidate->mType == FMOD_DSP_TYPE_LOWPASS_SIMPLE)
            require(typeid(*instance) == typeid(FMOD::DSPLowPassSimple));
        else if (candidate->mType == FMOD_DSP_TYPE_CHORUS)
            require(typeid(*instance) == typeid(FMOD::DSPChorus));
        else if (candidate->mType == FMOD_DSP_TYPE_ECHO)
            require(typeid(*instance) == typeid(FMOD::DSPEcho));
        else if (candidate->mType == FMOD_DSP_TYPE_LOWPASS)
            require(typeid(*instance) == typeid(FMOD::DSPLowPass));
        else if (candidate->mType == FMOD_DSP_TYPE_ITLOWPASS)
            require(typeid(*instance) == typeid(FMOD::DSPLowPass2));
        else if (candidate->mType == FMOD_DSP_TYPE_HIGHPASS)
            require(typeid(*instance) == typeid(FMOD::DSPHighPass));
        else if (candidate->mType == FMOD_DSP_TYPE_PARAMEQ)
            require(typeid(*instance) == typeid(FMOD::DSPParamEq));
        else require(typeid(*instance) == typeid(FMOD::DSPFilter));
        require(instance->release() == FMOD_OK);
    }
    require(desc && desc->mCategory == FMOD::FMOD_DSP_CATEGORY_FILTER);
    require(desc->mSize == sizeof(FMOD::DSPPitchShift));
    require(desc->create == FMOD::DSPPitchShift::createCallback);
    require(desc->release == FMOD::DSPPitchShift::releaseCallback);
    require(desc->reset == FMOD::DSPPitchShift::resetCallback);
    require(desc->read == FMOD::DSPPitchShift::readCallback);
    require(desc->setparameter == FMOD::DSPPitchShift::setParameterCallback);
    require(desc->getparameter == FMOD::DSPPitchShift::getParameterCallback);
    const int baseline = live;
    FMOD::DSPI* dsp = nullptr;
    require(factory->createDSP(nullptr, &dsp) == FMOD_ERR_INVALID_PARAM);
    require(factory->createDSP(desc, nullptr) == FMOD_ERR_INVALID_PARAM);
    // Fail object storage, then the actual per-channel allocation during create.
    for (int allocation : {0, 1}) {
        failAfter = allocation;
        require(internal->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp) == FMOD_ERR_MEMORY);
        require(!dsp && failAfter == -1 && live == baseline);
    }
    for (int rate : {22050, 48000}) {
        require(system->setSoftwareFormat(rate, FMOD_SOUND_FORMAT_PCMFLOAT, 2, 0, FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
        require(internal->createDSPByType(FMOD_DSP_TYPE_PITCHSHIFT, &dsp) == FMOD_OK);
        receiver(dsp);
        require(dsp->instance == (FMOD_DSP*)dsp && dsp->mSystem == internal);
        require(live == baseline + 2); // Object plus channel/FFT working state.
        defaults(dsp);
        processing(dsp, rate, 2, 1024, 1.0f);
        parameters(dsp);
        for (int fft : {256, 512, 1024, 2048, 4096})
            for (float pitch : {0.5f, 1.0f, 2.0f}) processing(dsp, rate, 2, fft, pitch);
        processing(dsp, rate, 1, 1024, 1.0f);
        // More input channels than configured retains the original bypass copy.
        float input[] = {0.25f, -0.5f, 1, 0}, output[4] = {};
        require(dsp->mDescription.read(dsp, input, output, 2, 2, 2) == FMOD_OK);
        require(!std::memcmp(input, output, sizeof(input)));
        require(dynamic_cast<FMOD::DSPPitchShift*>(dsp)->startBuffering(64) == FMOD_OK);
        require(dsp->release() == FMOD_OK && live == baseline);
        dsp = nullptr;
    }
    // Actual alloc validation failures and partial creation failure, both owned
    // and preconstructed routes. The factory retains its existing error ownership.
    for (bool preconstructed : {false, true}) {
        for (int mode = 0; mode < 4; ++mode) {
            auto bad = *desc;
            bad.channels = mode == 0 ? -1 : mode == 1 ? 100 : 0;
            if (mode == 2) bad.setparameter = rejectParameter;
            bad.create = observeCreate;
            bad.release = observeRelease;
            dsp = preconstructed ? FMOD_Object_Calloc(FMOD::DSPPitchShift) : nullptr;
            if (preconstructed) require(dsp);
            if (mode == 3) failAfter = preconstructed ? 0 : 1;
            const auto error = mode == 0 ? FMOD_ERR_INVALID_PARAM :
                               mode == 1 ? FMOD_ERR_TOOMANYCHANNELS :
                               mode == 2 ? FMOD_ERR_INTERNAL : FMOD_ERR_MEMORY;
            require(factory->createDSP(&bad, &dsp) == error);
            require(live == baseline && released == 0);
            dsp = nullptr; // Preconstructed storage is freed on factory error.
        }
    }
    require(created == 4 && failAfter == -1);
    auto observed = *desc;
    observed.create = observeCreate;
    observed.release = observeRelease;
    dsp = FMOD_Object_Calloc(FMOD::DSPPitchShift);
    require(dsp && factory->createDSP(&observed, &dsp) == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && released == 1 && live == baseline + 1);
    receiver(dsp); // release(false) keeps the concrete object live.
    // Allocation failure while replacing channel buffers leaves no stale owner.
    require(observed.create(dsp) == FMOD_OK && live == baseline + 2);
    failAfter = 0;
    require(dsp->setParameter(3, 16) == FMOD_ERR_MEMORY && failAfter == -1);
    require(live == baseline + 1);
    require(dsp->reset() == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && released == 2 && live == baseline + 1);
    require(dsp->release(false) == FMOD_OK && released == 3 && live == baseline + 1);
    // Reinitialize the retained object through the real create callback.
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
    std::puts("native-pitchshift=passed: registered dynamic type/callbacks, FFT processing/pitch/reset/parameters, owned buffers, creation/allocation failures, release(false), repeated initialization and FILTER inventory");
}
