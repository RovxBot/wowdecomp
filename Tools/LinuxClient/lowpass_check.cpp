// Actual registered FMOD factory, lowpass callbacks and pool ownership.
// Failure injection and release observation are confined to this executable.
#include "fmod.hpp"
#include "fmod_dsp_lowpass_simple.h"
#include "fmod_memory.h"
#include "fmod_pluginfactory.h"
#include "fmod_systemi.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <typeinfo>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "lowpass check failed at line %d\n", line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)

static int live = 0, released = 0, rejected = 0;
static bool failAllocation = false;
static void* F_CALLBACK allocate(unsigned size, FMOD_MEMORY_TYPE) {
    if (failAllocation) { failAllocation = false; return nullptr; }
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

// Release/strict use FMOD's non-DEBUG class layouts. Trivial destruction lets
// the existing raw pool frees end the concrete lifetime after explicit cleanup.
static_assert(std::is_trivially_destructible<FMOD::DSPLowPassSimple>::value,
              "Reaudit pool cleanup if the lowpass acquires destructor-owned resources");

static FMOD_RESULT F_CALLBACK releaseCallback(FMOD_DSP_STATE* state) {
    auto* dsp = dynamic_cast<FMOD::DSPLowPassSimple*>(static_cast<FMOD::DSPI*>(state));
    require(dsp && typeid(*dsp) == typeid(FMOD::DSPLowPassSimple));
    float value = 0;
    char text[32] = {};
    require(dsp->getParameter(0, &value, text, sizeof(text)) == FMOD_OK);
    require(value == 5000.0f); // Callback still sees a live initialized filter.
    ++released;
    return FMOD_OK;
}
static FMOD_RESULT F_CALLBACK rejectCreate(FMOD_DSP_STATE* state) {
    require(dynamic_cast<FMOD::DSPLowPassSimple*>(static_cast<FMOD::DSPI*>(state)));
    require(FMOD::DSPLowPassSimple::createCallback(state) == FMOD_OK);
    ++rejected;
    return FMOD_ERR_INTERNAL;
}

static void filtering(FMOD::DSPI* dsp, int rate) {
    const auto& desc = dsp->mDescription;
    float value = 0;
    char text[32] = {};
    require(dsp->getParameter(0, &value, text, sizeof(text)) == FMOD_OK && value == 5000.0f);
    // Real callbacks: reset, coefficient updates, independent channel histories,
    // impulse decay and the original two-pole RC response, including clamping.
    for (int channels : {1, 2, 6}) {
        for (float cutoff : {10.0f, 5000.0f, 20000.0f}) {
            require(dsp->setParameter(0, cutoff) == FMOD_OK);
            require(dsp->reset() == FMOD_OK);
            float input[32 * 6] = {}, output[32 * 6] = {};
            for (int ch = 0; ch < channels; ++ch) input[ch] = float(ch + 1);
            require(desc.read(dsp, input, output, 32, channels, channels) == FMOD_OK);
            const float threshold = rate / 3.14159265358979323846f;
            const float dt = 1.0f / rate;
            const float rc = 1.0f / (2.0f * FMOD_PI * cutoff);
            const float tc = cutoff <= threshold ? dt / (rc + dt) :
                0.666666667f + (cutoff - threshold) / (3.0f * (22000.0f - threshold));
            for (int ch = 0; ch < channels; ++ch) {
                float first = 0, second = 0;
                for (int frame = 0; frame < 32; ++frame) {
                    first = tc * input[frame * channels + ch] + (1 - tc) * first;
                    second = tc * first + (1 - tc) * second;
                    require(std::isfinite(output[frame * channels + ch]));
                    require(std::fabs(output[frame * channels + ch] - second) < 0.00001f);
                }
            }
            require(dsp->reset() == FMOD_OK);
            std::memset(input, 0, sizeof(input));
            require(desc.read(dsp, input, output, 32, channels, channels) == FMOD_OK);
            for (int i = 0; i < 32 * channels; ++i) require(std::fabs(output[i]) < 1e-18f);
        }
    }
    require(dsp->setParameter(0, -1.0f) == FMOD_OK);
    require(dsp->getParameter(0, &value, text, sizeof(text)) == FMOD_OK && value == 10.0f);
    require(dsp->setParameter(0, 99999.0f) == FMOD_OK);
    require(dsp->getParameter(0, &value, text, sizeof(text)) == FMOD_OK && value == 22000.0f);
    float input[] = {0.25f, -0.5f, 1.0f, 0.0f}, output[4] = {};
    require(desc.read(dsp, input, output, 2, 2, 2) == FMOD_OK);
    require(std::memcmp(input, output, sizeof(input)) == 0);
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
    for (int i = 0; i < count; ++i) {
        FMOD::FMOD_DSP_DESCRIPTION_EX* candidate = nullptr;
        require(factory->getDSP(i, &candidate) == FMOD_OK);
        if (candidate->mType == FMOD_DSP_TYPE_LOWPASS_SIMPLE) desc = candidate;
    }
    require(desc && desc->mCategory == FMOD::FMOD_DSP_CATEGORY_FILTER);
    require(desc->mSize == sizeof(FMOD::DSPLowPassSimple));
    require(desc->create == FMOD::DSPLowPassSimple::createCallback);
    require(desc->reset == FMOD::DSPLowPassSimple::resetCallback);
    require(desc->read == FMOD::DSPLowPassSimple::readCallback);
    require(desc->setparameter == FMOD::DSPLowPassSimple::setParameterCallback);
    require(desc->getparameter == FMOD::DSPLowPassSimple::getParameterCallback);
    require(!desc->release);
    const int baseline = live;
    FMOD::DSPI* dsp = nullptr;
    require(factory->createDSP(nullptr, &dsp) == FMOD_ERR_INVALID_PARAM);
    require(factory->createDSP(desc, nullptr) == FMOD_ERR_INVALID_PARAM);
    failAllocation = true;
    require(internal->createDSPByType(FMOD_DSP_TYPE_LOWPASS_SIMPLE, &dsp) == FMOD_ERR_MEMORY && !dsp);
    require(!failAllocation && live == baseline);

    for (int rate : {22050, 48000}) {
        require(system->setSoftwareFormat(rate, FMOD_SOUND_FORMAT_PCMFLOAT, 2, 0, FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
        require(internal->createDSPByType(FMOD_DSP_TYPE_LOWPASS_SIMPLE, &dsp) == FMOD_OK);
        require(typeid(*dsp) == typeid(FMOD::DSPLowPassSimple));
        auto* lowpass = dynamic_cast<FMOD::DSPLowPassSimple*>(dsp);
        require(lowpass && dsp->instance == (FMOD_DSP*)dsp && dsp->mSystem == internal);
        filtering(dsp, rate);
        require(lowpass->startBuffering(64) == FMOD_OK && live > baseline);
        require(dsp->release() == FMOD_OK && live == baseline);
        dsp = nullptr;
    }

    // Failed alloc and failed create return their actual errors, without leaking
    // pool storage or calling the successful-release callback on partial init.
    for (int mode = 0; mode < 3; ++mode) {
        auto bad = *desc;
        bad.channels = mode == 0 ? -1 : mode == 1 ? 100 : 0;
        bad.create = rejectCreate;
        const auto error = mode == 0 ? FMOD_ERR_INVALID_PARAM :
                           mode == 1 ? FMOD_ERR_TOOMANYCHANNELS : FMOD_ERR_INTERNAL;
        require(factory->createDSP(&bad, &dsp) == error && !dsp && live == baseline);
        dsp = FMOD_Object_Calloc(FMOD::DSPLowPassSimple);
        require(dsp);
        require(factory->createDSP(&bad, &dsp) == error);
        require(live == baseline);
        dsp = nullptr; // The factory's existing preconstructed contract frees on error.
    }
    require(rejected == 2 && released == 0);

    auto observed = *desc;
    observed.release = releaseCallback;
    dsp = FMOD_Object_Calloc(FMOD::DSPLowPassSimple);
    require(dsp && factory->createDSP(&observed, &dsp) == FMOD_OK);
    require(dynamic_cast<FMOD::DSPLowPassSimple*>(dsp)->startBuffering(64) == FMOD_OK);
    require(dsp->release(false) == FMOD_OK && released == 1);
    require(typeid(*dsp) == typeid(FMOD::DSPLowPassSimple) && live == baseline + 1);
    require(dsp->release() == FMOD_OK && released == 2 && live == baseline);

    // Generic filter/head units must keep their original base construction.
    FMOD::FMOD_DSP_DESCRIPTION_EX generic = {};
    generic.mCategory = FMOD::FMOD_DSP_CATEGORY_FILTER;
    generic.mFormat = FMOD_SOUND_FORMAT_PCMFLOAT;
    dsp = nullptr;
    require(factory->createDSP(&generic, &dsp) == FMOD_OK);
    require(typeid(*dsp) == typeid(FMOD::DSPFilter));
    require(dsp->release() == FMOD_OK && live == baseline);
    require(internal->mConnectionRequestUsedHead.isEmpty());
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(FMOD_OS_CriticalSection_Free(*slot) == FMOD_OK);
        *slot = nullptr;
    }
    require(system->release() == FMOD_OK && live == 0);
    require(FMOD_Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr) == FMOD_OK);
    std::puts("native-lowpass=passed: registered factory/type, original RC filtering/reset/parameters, history cleanup, allocation/alloc/create failures, trivial lifetime and nonfree release");
}
