// Actual registered inline-state recursive filters. No replacement DSP/mixer.
#include "fmod.hpp"
#include "fmod_dsp_lowpass.h"
#include "fmod_dsp_lowpass2.h"
#include "fmod_dsp_highpass.h"
#include "fmod_dsp_parameq.h"
#include "fmod_memory.h"
#include "fmod_pluginfactory.h"
#include "fmod_systemi.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <type_traits>
#include <typeinfo>
#include <vector>

static const char* current;
static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "%s check failed at line %d\n", current, line);
        std::abort();
    }
}
#define require(value) check((value), __LINE__)
static int live = 0, failAfter = -1, created = 0, released = 0, rejectIndex = -1;
static void* F_CALLBACK allocate(unsigned size, FMOD_MEMORY_TYPE) {
    if (failAfter >= 0 && failAfter-- == 0) return nullptr;
    void* p = std::malloc(size);
    if (p) ++live;
    return p;
}
static void* F_CALLBACK reallocate(void* p, unsigned size, FMOD_MEMORY_TYPE) {
    if (!size) { if (p) --live; std::free(p); return nullptr; }
    const bool fresh = !p;
    void* result = std::realloc(p, size);
    if (result && fresh) ++live;
    return result;
}
static void F_CALLBACK deallocate(void* p, FMOD_MEMORY_TYPE) {
    require(p && live > 0);
    --live;
    std::free(p);
}

template<class T> static T* receiver(FMOD_DSP_STATE* state) {
    auto* p = dynamic_cast<T*>(static_cast<FMOD::DSPI*>(state));
    require(p && typeid(*p) == typeid(T));
    require(reinterpret_cast<std::uintptr_t>(p) % alignof(T) == 0);
    return p;
}
template<class T> static FMOD_RESULT F_CALLBACK observeCreate(FMOD_DSP_STATE* state) {
    receiver<T>(state);
    ++created;
    return T::createCallback(state);
}
template<class T> static FMOD_RESULT F_CALLBACK observeRelease(FMOD_DSP_STATE* state) {
    // The four real descriptions have no release callback. Observe base cleanup
    // while the concrete lifetime is still live, without inventing DSP cleanup.
    receiver<T>(state);
    ++released;
    return FMOD_OK;
}
template<class T> static FMOD_RESULT F_CALLBACK rejectParameter(FMOD_DSP_STATE* s, int i, float v) {
    receiver<T>(s);
    if (i == rejectIndex) return FMOD_ERR_INTERNAL;
    return T::setParameterCallback(s, i, v);
}
static float parameter(FMOD::DSPI* dsp, int index) {
    float value = -1;
    char text[32] = {};
    require(dsp->getParameter(index, &value, text, sizeof(text)) == FMOD_OK && text[0]);
    require(dsp->getParameter(index, nullptr, nullptr, 0) == FMOD_OK);
    return value;
}
static void defaults(FMOD::DSPI* dsp) {
    for (int i = 0; i < dsp->mDescription.numparameters; ++i)
        require(parameter(dsp, i) == dsp->mDescription.paramdesc[i].defaultval);
}
static void parameters(FMOD::DSPI* dsp) {
    const int count = dsp->mDescription.numparameters;
    for (int i : {-1, count + 1}) { // #202's count boundary stays separate.
        require(dsp->setParameter(i, 1) == FMOD_ERR_INVALID_PARAM);
        require(dsp->getParameter(i, nullptr, nullptr, 0) == FMOD_ERR_INVALID_PARAM);
    }
    for (int i = 0; i < count; ++i) {
        const auto& p = dsp->mDescription.paramdesc[i];
        for (float v : {p.min - 1, p.min, p.max, p.max + 1, p.defaultval}) {
            require(dsp->setParameter(i, v) == FMOD_OK);
            require(parameter(dsp, i) == std::clamp(v, p.min, p.max));
        }
    }
    defaults(dsp); // At 48kHz no additional Highpass Nyquist clamp is needed.
}

// Independent steady-state transfer functions, rather than another sample loop.
// Each owner is checked against its own original filter, including IT damping
// and ParamEQ's linear A (its centre gain is A*A).
static double gain(FMOD_DSP_TYPE type, int rate, double frequency) {
    constexpr double pi = 3.14159265358979323846;
    const double cutoff = rate / 32.0;
    const auto z = std::polar(1.0, -2 * pi * frequency / rate);
    if (type == FMOD_DSP_TYPE_LOWPASS) {
        const std::complex<double> s(0, std::tan(pi * frequency / rate) / std::tan(pi * cutoff / rate));
        return std::abs(1.0 / ((s*s + 0.765367*s + 1.0) * (s*s + 1.847759*s + 1.0)));
    }
    if (type == FMOD_DSP_TYPE_ITLOWPASS) {
        const double fc = 2 * pi * cutoff / rate;
        const double damping = std::pow(10.0, -(24.0 / 128) / 20);
        const double d = (2 * damping - std::min(2.0, (1 - 2 * damping) * fc)) / fc;
        const double e = 1 / (fc * fc), denominator = 1 + d + e;
        return std::abs((1 / denominator) / (1.0 - ((d + 2*e) / denominator)*z + (e / denominator)*z*z));
    }
    const double w = 2 * pi * cutoff / rate;
    if (type == FMOD_DSP_TYPE_HIGHPASS) {
        const double b = (1 + std::cos(w)) / 2, a = std::sin(w) / 2;
        return std::abs((b - 2*b*z + b*z*z) / (1 + a - 2*std::cos(w)*z + (1-a)*z*z));
    }
    const double alpha = std::sin(w) / 2;
    return std::abs((1 + alpha*2 - 2*std::cos(w)*z + (1-alpha*2)*z*z) /
                    (1 + alpha/2 - 2*std::cos(w)*z + (1-alpha/2)*z*z));
}
static void configure(FMOD::DSPI* dsp, FMOD_DSP_TYPE type, int rate) {
    require(dsp->setParameter(0, float(rate) / 32) == FMOD_OK);
    require(dsp->setParameter(1, 1) == FMOD_OK);
    if (type == FMOD_DSP_TYPE_PARAMEQ) require(dsp->setParameter(2, 2) == FMOD_OK);
}
template<class T> static void processing(FMOD::SystemI* system, FMOD_DSP_TYPE type, int rate, int channels) {
    constexpr int frames = 16384;
    for (int period : {128, 8}) {
        FMOD::DSPI *whole = nullptr, *split = nullptr, *inplace = FMOD_Object_Calloc(T);
        require(inplace);
        // Compare factory-owned and genuinely preconstructed concrete paths.
        require(system->createDSPByType(type, &whole) == FMOD_OK);
        require(system->createDSPByType(type, &split) == FMOD_OK);
        require(system->mPluginFactory->createDSP(T::getDescriptionEx(), &inplace) == FMOD_OK);
        for (auto* p : {whole, split, inplace}) { receiver<T>(p); defaults(p); configure(p, type, rate); }
        std::vector<float> input(frames*channels), a(input.size()), b(input.size()), c;
        for (int i = 0; i < frames; ++i)
            for (int ch = 0; ch < channels; ++ch)
                input[i*channels+ch] = ch == 1 ? 0 : float(0.125 * std::sin(2*3.141592653589793*i/period) / (ch+1));
        c = input;
        require(whole->mDescription.read(whole, input.data(), a.data(), frames, channels, channels) == FMOD_OK);
        for (int i = 0; i < frames; i += 113)
            require(split->mDescription.read(split, input.data()+i*channels, b.data()+i*channels,
                    std::min(113, frames-i), channels, channels) == FMOD_OK);
        require(inplace->mDescription.read(inplace, c.data(), c.data(), frames, channels, channels) == FMOD_OK);
        double inputEnergy = 0, outputEnergy = 0;
        for (int i = 0; i < frames; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                const int n = i*channels+ch;
                require(std::isfinite(a[n]) && std::fabs(a[n]-b[n]) < 0.000002f && std::fabs(a[n]-c[n]) < 0.000002f);
                if (ch == 1) require(std::fabs(a[n]) < 1e-12f);
            }
            if (i >= frames/2) { inputEnergy += input[i*channels]*input[i*channels]; outputEnergy += a[i*channels]*a[i*channels]; }
        }
        const double measured = std::sqrt(outputEnergy/inputEnergy), expected = gain(type, rate, double(rate)/period);
        std::printf("%s rate=%d channels=%d period=%d gain=%.9g expected=%.9g\n", current, rate, channels, period, measured, expected);
        require(std::fabs(measured-expected) < 0.002 * std::max(0.01, expected));
        // Parameter changes reach processing, including each owner's state update.
        for (auto* p : {whole, split, inplace}) require(p->setParameter(0, float(rate)/16) == FMOD_OK);
        c = input;
        require(whole->mDescription.read(whole, input.data(), a.data(), frames, channels, channels) == FMOD_OK);
        require(split->mDescription.read(split, input.data(), b.data(), frames, channels, channels) == FMOD_OK);
        require(inplace->mDescription.read(inplace, c.data(), c.data(), frames, channels, channels) == FMOD_OK);
        for (std::size_t i = 0; i < a.size(); ++i)
            require(std::isfinite(a[i]) && std::fabs(a[i]-b[i]) < 0.000002f && std::fabs(a[i]-c[i]) < 0.000002f);
        for (auto* p : {whole, split, inplace}) {
            const bool reset = type != FMOD_DSP_TYPE_LOWPASS;
            require(p->reset() == (reset ? FMOD_OK : FMOD_ERR_UNSUPPORTED));
            // Full Lowpass has no reset callback: preserve that contract.
            if (reset) {
                std::fill(a.begin(), a.end(), 0);
                require(p->mDescription.read(p, a.data(), a.data(), frames, channels, channels) == FMOD_OK);
                for (float v : a) require(std::fabs(v) < 1e-12f);
            }
            require(p->release() == FMOD_OK);
        }
    }
}

template<class T> static void run(FMOD_DSP_TYPE type) {
    static_assert(std::is_trivially_destructible<T>::value, "Reaudit destructor ownership");
    static_assert(std::is_trivially_default_constructible<T>::value == false, "Base constructor must run");
    static_assert(alignof(T) <= alignof(std::max_align_t), "Reaudit pool alignment");
    require(FMOD_Memory_Initialize(nullptr, 0, allocate, reallocate, deallocate) == FMOD_OK);
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    FMOD::SystemI* internal = nullptr;
    require(FMOD::SystemI::validate(system, &internal) == FMOD_OK);
    require(internal->setUpPlugins() == FMOD_OK);
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit, &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit})
        require(!*slot && FMOD_OS_CriticalSection_Create(slot) == FMOD_OK);
    internal->mConnectionRequestUsedHead.initNode(); internal->mConnectionRequestFreeHead.initNode();
    for (auto& r : internal->mConnectionRequest) { r.initNode(); r.addBefore(&internal->mConnectionRequestFreeHead); }
    auto* factory = internal->mPluginFactory;
    FMOD::FMOD_DSP_DESCRIPTION_EX* desc = nullptr;
    int count = 0;
    require(factory->getNumDSPs(&count) == FMOD_OK);
    for (int i = 0; i < count; ++i) {
        FMOD::FMOD_DSP_DESCRIPTION_EX* candidate = nullptr;
        require(factory->getDSP(i, &candidate) == FMOD_OK);
        if (candidate->mType == type) desc = candidate;
    }
    require(desc && desc->mCategory == FMOD::FMOD_DSP_CATEGORY_FILTER && desc->mSize == sizeof(T));
    require(desc->create == T::createCallback && !desc->release && desc->read == T::readCallback);
    require(desc->setparameter == T::setParameterCallback && desc->getparameter == T::getParameterCallback);
    const int baseline = live;
    FMOD::DSPI* dsp = nullptr;
    require(factory->createDSP(nullptr, &dsp) == FMOD_ERR_INVALID_PARAM);
    require(factory->createDSP(desc, nullptr) == FMOD_ERR_INVALID_PARAM);
    failAfter = 0;
    require(internal->createDSPByType(type, &dsp) == FMOD_ERR_MEMORY && !dsp && live == baseline && failAfter == -1);
    for (int rate : {22050, 48000}) for (int channels : {1, 2, 3, 6, 8}) {
        require(system->setSoftwareFormat(rate, FMOD_SOUND_FORMAT_PCMFLOAT, channels, 0, FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
        processing<T>(internal, type, rate, channels);
        require(live == baseline);
    }
    require(system->setSoftwareFormat(48000, FMOD_SOUND_FORMAT_PCMFLOAT, 2, 0, FMOD_DSP_RESAMPLER_LINEAR) == FMOD_OK);
    // Both failure paths free the object without a release callback. There is
    // no DSP-owned allocation to unwind for these four inline-state owners.
    for (bool preconstructed : {false, true}) for (int mode = -2; mode < desc->numparameters; ++mode) {
        auto bad = *desc;
        bad.create = observeCreate<T>; bad.release = observeRelease<T>;
        bad.channels = mode == -2 ? -1 : mode == -1 ? 100 : 0;
        rejectIndex = mode; bad.setparameter = rejectParameter<T>;
        dsp = preconstructed ? FMOD_Object_Calloc(T) : nullptr;
        if (preconstructed) require(dsp);
        const auto error = mode == -2 ? FMOD_ERR_INVALID_PARAM : mode == -1 ? FMOD_ERR_TOOMANYCHANNELS : FMOD_ERR_INTERNAL;
        require(factory->createDSP(&bad, &dsp) == error && live == baseline && released == 0);
        dsp = nullptr; // Existing preconstructed-error contract frees this storage.
    }
    require(created == 2*desc->numparameters);
    auto observed = *desc;
    observed.mSize = 0; observed.create = observeCreate<T>; observed.release = observeRelease<T>;
    require(factory->createDSP(&observed, &dsp) == FMOD_OK && live == baseline+1);
    auto* concrete = receiver<T>(dsp);
    require(dsp->instance == (FMOD_DSP*)dsp && dsp->mSystem == internal);
    defaults(dsp); parameters(dsp);
    for (int repeat = 0; repeat < 3; ++repeat) {
        failAfter = 0;
        require(concrete->startBuffering(64) == FMOD_ERR_MEMORY && failAfter == -1 && live == baseline+1);
        require(concrete->startBuffering(64) == FMOD_OK && live == baseline+2);
        float* history = nullptr; unsigned position = 99, length = 0;
        require(concrete->getHistoryBuffer(&history, &position, &length) == FMOD_OK && history && position == 0 && length == 64);
        require(concrete->startBuffering(64) == FMOD_OK && live == baseline+2);
        // Base-owned mix buffer must also be freed, independently of history.
        dsp->mBufferMemory = (float*)FMOD_Memory_Calloc(128);
        require(dsp->mBufferMemory && live == baseline+3);
        require(dsp->release(false) == FMOD_OK && live == baseline+1 && released == 2*repeat+1);
        receiver<T>(dsp);
        require(!dsp->mBufferMemory);
        require(concrete->getHistoryBuffer(&history, nullptr, nullptr) == FMOD_OK && !history);
        require(dsp->release(false) == FMOD_OK && live == baseline+1 && released == 2*repeat+2);
        require(dsp->setParameter(0, 1000) == FMOD_OK);
        // Reinitialize the retained live object; preserve full Lowpass's inline
        // history retention, while the other owners reset their history.
        require(factory->createDSP(&observed, &dsp) == FMOD_OK && receiver<T>(dsp) == concrete);
        defaults(dsp);
    }
    require(dsp->release() == FMOD_OK && live == baseline && released == 7);
    FMOD::FMOD_DSP_DESCRIPTION_EX generic = {};
    generic.mCategory = FMOD::FMOD_DSP_CATEGORY_FILTER; generic.mFormat = FMOD_SOUND_FORMAT_PCMFLOAT;
    dsp = nullptr;
    require(factory->createDSP(&generic, &dsp) == FMOD_OK && typeid(*dsp) == typeid(FMOD::DSPFilter));
    require(dsp->release() == FMOD_OK && live == baseline);
    require(internal->mConnectionRequestUsedHead.isEmpty());
    for (auto slot : {&internal->mDSPCrit, &internal->mDSPLockCrit, &internal->mDSPConnectionCrit, &internal->mDSPRequestCrit}) {
        require(FMOD_OS_CriticalSection_Free(*slot) == FMOD_OK); *slot = nullptr;
    }
    require(system->release() == FMOD_OK && live == 0);
    require(FMOD_Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr) == FMOD_OK);
    std::printf("%s=passed: actual factory/callbacks/processing/parameters, failures, history/base ownership, release(false), reinitialization and pool balance\n", current);
}
int main(int argc, char** argv) {
    require(argc == 2); current = argv[1];
    if (!std::strcmp(current, "lowpass_full")) run<FMOD::DSPLowPass>(FMOD_DSP_TYPE_LOWPASS);
    else if (!std::strcmp(current, "itlowpass")) run<FMOD::DSPLowPass2>(FMOD_DSP_TYPE_ITLOWPASS);
    else if (!std::strcmp(current, "highpass")) run<FMOD::DSPHighPass>(FMOD_DSP_TYPE_HIGHPASS);
    else if (!std::strcmp(current, "parameq")) run<FMOD::DSPParamEq>(FMOD_DSP_TYPE_PARAMEQ);
    else require(false);
}
