// Link the actual FMOD DSPI/public API and registered FILTER implementations.
#include "fmod.hpp"
#include "fmod_dspi.h"
#include "fmod_systemi.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

static void check(bool value, int line) {
    if (!value) { std::fprintf(stderr, "parameter check failed at %d\n", line); std::abort(); }
}
#define require(value) check((value), __LINE__)

static int sets, gets, seenIndex;
static float seenValue;
static FMOD_RESULT callbackResult = FMOD_OK;
static FMOD_RESULT F_CALLBACK setter(FMOD_DSP_STATE* state, int index, float value) {
    require(state->instance == reinterpret_cast<FMOD_DSP*>(static_cast<FMOD::DSPI*>(state)));
    ++sets; seenIndex = index; seenValue = value;
    return callbackResult;
}
static FMOD_RESULT F_CALLBACK getter(FMOD_DSP_STATE* state, int index, float* value, char* text) {
    require(state->instance == reinterpret_cast<FMOD_DSP*>(static_cast<FMOD::DSPI*>(state)));
    require(value && text); // DSPI always supplies local outputs, even for null callers.
    ++gets; seenIndex = index;
    *value = seenValue;
    std::strcpy(text, "0123456789abcdefghijkl");
    return callbackResult;
}

template<class DSP> static void invalid(DSP* dsp, int count) {
    for (int index : {-1, count, count+1}) {
        const int oldSets = sets, oldGets = gets;
        require(dsp->setParameter(index, 1) == FMOD_ERR_INVALID_PARAM);
        for (bool numeric : {false, true}) for (bool textual : {false, true}) {
            float value = -123;
            char text[32], before[32];
            std::memset(text, '?', sizeof(text)); std::memcpy(before, text, sizeof(text));
            require(dsp->getParameter(index, numeric ? &value : nullptr,
                    textual ? text : nullptr, sizeof(text)) == FMOD_ERR_INVALID_PARAM);
            require(value == -123 && !std::memcmp(text, before, sizeof(text)));
        }
        require(sets == oldSets && gets == oldGets);
    }
}

static void sharedContract() {
    FMOD::DSPI dsp;
    dsp.mDescription = {};
    dsp.mDescription.setparameter = setter;
    dsp.mDescription.getparameter = getter;
    // A null parameter array makes any attempted description access observable.
    invalid(&dsp, 0); // Includes -1, 0 == count, and count+1.
    FMOD_DSP_PARAMETERDESC descriptions[] = {
        {-2, 3, 1, "First", "", "First parameter"},
        {10, 20, 15, "Last", "", "Last parameter"}
    };
    dsp.mDescription.paramdesc = descriptions;
    dsp.mDescription.numparameters = 2;
    invalid(&dsp, 2);
    for (int index : {0, 1}) {
        const auto& desc = descriptions[index];
        for (float value : {desc.min-1, desc.min, desc.defaultval, desc.max, desc.max+1}) {
            int before = sets;
            require(dsp.setParameter(index, value) == FMOD_OK && sets == before+1);
            require(seenIndex == index && seenValue == std::clamp(value, desc.min, desc.max));
            for (bool numeric : {false, true}) for (bool textual : {false, true})
                for (int length : {0, 1, 4, 16, 32}) {
                    float output = -123;
                    char text[32]; std::memset(text, '?', sizeof(text));
                    before = gets;
                    require(dsp.getParameter(index, numeric ? &output : nullptr,
                            textual ? text : nullptr, length) == FMOD_OK && gets == before+1);
                    require(seenIndex == index && output == (numeric ? seenValue : -123));
                    const int copied = textual ? std::min(length, 16) : 0;
                    require(!std::memcmp(text, "0123456789abcdefghijkl", copied));
                    for (int i = copied; i < 32; ++i) require(text[i] == '?');
                }
        }
    }
    callbackResult = FMOD_ERR_INTERNAL;
    require(dsp.setParameter(0, 100) == FMOD_ERR_INTERNAL && seenValue == 3);
    for (bool numeric : {false, true}) for (bool textual : {false, true}) {
        float value = -123;
        char text[32], before[32];
        std::memset(text, '?', sizeof(text)); std::memcpy(before, text, sizeof(text));
        require(dsp.getParameter(1, numeric ? &value : nullptr, textual ? text : nullptr,
                sizeof(text)) == FMOD_ERR_INTERNAL);
        require(value == -123 && !std::memcmp(text, before, sizeof(text)));
    }
    invalid(&dsp, 2); // Index rejection takes precedence over callback errors.
    callbackResult = FMOD_OK;
    dsp.mDescription.setparameter = nullptr;
    dsp.mDescription.getparameter = nullptr;
    for (int index : {-1, 0, 1, 2, 3}) {
        float value = -123; char text[] = "unchanged";
        require(dsp.setParameter(index, 1) == FMOD_ERR_UNSUPPORTED);
        require(dsp.getParameter(index, &value, text, sizeof(text)) == FMOD_ERR_UNSUPPORTED);
        require(value == -123 && !std::strcmp(text, "unchanged"));
    }
}

static void registeredFilters() {
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    FMOD::SystemI* internal = nullptr;
    require(FMOD::SystemI::validate(system, &internal) == FMOD_OK);
    require(internal->setUpPlugins() == FMOD_OK);
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit})
        require(!*slot && FMOD_OS_CriticalSection_Create(slot) == FMOD_OK);
    internal->mConnectionRequestUsedHead.initNode(); internal->mConnectionRequestFreeHead.initNode();
    for (auto& r : internal->mConnectionRequest) { r.initNode(); r.addBefore(&internal->mConnectionRequestFreeHead); }
    for (auto type : {FMOD_DSP_TYPE_LOWPASS_SIMPLE, FMOD_DSP_TYPE_PITCHSHIFT,
                     FMOD_DSP_TYPE_CHORUS, FMOD_DSP_TYPE_ECHO, FMOD_DSP_TYPE_LOWPASS,
                     FMOD_DSP_TYPE_ITLOWPASS, FMOD_DSP_TYPE_HIGHPASS, FMOD_DSP_TYPE_PARAMEQ}) {
        FMOD::DSP* dsp = nullptr;
        require(system->createDSPByType(type, &dsp) == FMOD_OK);
        FMOD::DSPI* actual = nullptr;
        require(FMOD::DSPI::validate(dsp, &actual) == FMOD_OK);
        int count = -1;
        require(dsp->getNumParameters(&count) == FMOD_OK && count > 0);
        require(count == actual->mDescription.numparameters);
        invalid(dsp, count);
        for (int index : {0, count-1}) {
            const auto& desc = actual->mDescription.paramdesc[index];
            require(dsp->setParameter(index, desc.defaultval) == FMOD_OK);
            for (bool numeric : {false, true}) for (bool textual : {false, true}) {
                float value = -123; char text[32] = {};
                require(dsp->getParameter(index, numeric ? &value : nullptr,
                        textual ? text : nullptr, sizeof(text)) == FMOD_OK);
                if (numeric) require(std::isfinite(value) && value == desc.defaultval);
                if (textual) require(text[0]);
            }
        }
        require(dsp->release() == FMOD_OK);
    }
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit,
                      &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(FMOD_OS_CriticalSection_Free(*slot) == FMOD_OK); *slot = nullptr;
    }
    require(system->release() == FMOD_OK);
}
int main() {
    sharedContract();
    registeredFilters();
    std::puts("native-parameters=passed: actual DSPI/public FILTER APIs, count boundaries including zero, clamping, callback errors, nullable/bounded/unchanged outputs and unsupported precedence");
}
