// Exercise the registered FMOD factory and original ALSA callbacks, not a
// substitute output. Linker wrappers inject failures only in this executable.
#include "fmod.hpp"
#include "fmod_memory.h"
#include "fmod_output_alsa.h"
#include "fmod_pluginfactory.h"
#include "fmod_os_output.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <unistd.h>

static void check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ALSA output check failed at line %d\n", line);
        std::abort(); // Keep assertions active in release, too.
    }
}
#define require(value) check((value), __LINE__)

enum Failure { None, Parameters, Buffer, Writer };
static Failure failure = None;
static bool failAllocation = false;
static int live = 0, closes = 0, drops = 0, creates = 0, joins = 0;
static std::atomic<unsigned> writes{0};

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

extern "C" {
int __real_snd_pcm_set_params(snd_pcm_t*, snd_pcm_format_t, snd_pcm_access_t,
                              unsigned, unsigned, int, unsigned);
int __wrap_snd_pcm_set_params(snd_pcm_t* pcm, snd_pcm_format_t format, snd_pcm_access_t access,
                              unsigned channels, unsigned rate, int resample, unsigned latency) {
    if (failure == Parameters) return -EINVAL;
    int result = __real_snd_pcm_set_params(pcm, format, access, channels, rate, resample, latency);
    if (!result && failure == Buffer) failAllocation = true;
    return result;
}
int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*entry)(void*), void* data) {
    if (failure == Writer) return EAGAIN;
    int result = __real_pthread_create(thread, attr, entry, data);
    if (!result) ++creates;
    return result;
}
int __real_pthread_join(pthread_t, void**);
int __wrap_pthread_join(pthread_t thread, void** result) {
    int status = __real_pthread_join(thread, result);
    if (!status) ++joins;
    return status;
}
int __real_snd_pcm_drop(snd_pcm_t*);
int __wrap_snd_pcm_drop(snd_pcm_t* pcm) {
    require(creates == joins); // Writer must be joined before device teardown.
    ++drops;
    return __real_snd_pcm_drop(pcm);
}
int __real_snd_pcm_close(snd_pcm_t*);
int __wrap_snd_pcm_close(snd_pcm_t* pcm) {
    require(creates == joins);
    ++closes;
    return __real_snd_pcm_close(pcm);
}
snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t*, const void*, snd_pcm_uframes_t);
snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t* pcm, const void* data, snd_pcm_uframes_t frames) {
    // The real null PCM has no hardware pacing; bound this fixture's writer.
    usleep(1000);
    auto result = __real_snd_pcm_writei(pcm, data, frames);
    if (result > 0) ++writes;
    return result;
}
}

static FMOD::Output* create(FMOD::PluginFactory* factory, FMOD::FMOD_OUTPUT_DESCRIPTION_EX* description) {
    FMOD::Output* output = nullptr;
    require(factory->createOutput(description, &output) == FMOD_OK);
    require(output && typeid(*output) == typeid(FMOD::OutputALSA));
    require(dynamic_cast<FMOD::OutputALSA*>(output) != nullptr);
    require(output->readfrommixer == FMOD::Output::mixCallback);
    require(output->mDescription.init == FMOD::OutputALSA::initCallback);
    require(output->mDescription.close == FMOD::OutputALSA::closeCallback);
    int count = 0;
    char name[64] = {};
    FMOD_CAPS caps = FMOD_CAPS_NONE;
    auto& callbacks = output->mDescription;
    require(callbacks.getnumdrivers(output, &count) == FMOD_OK && count == 1);
    require(callbacks.getdrivername(output, 0, name, sizeof(name)) == FMOD_OK);
    require(std::strcmp(name, "ALSA Default") == 0);
    require(callbacks.getdrivercaps(output, 0, &caps) == FMOD_OK);
    require((caps & FMOD_CAPS_OUTPUT_FORMAT_PCM16) != 0);
    return output;
}

static FMOD_RESULT initialize(FMOD::Output* output, const char* device, int rate = 48000, int blocks = 4) {
    FMOD_SOUND_FORMAT format = FMOD_SOUND_FORMAT_NONE;
    auto result = output->mDescription.init(output, 0, FMOD_INIT_NORMAL, &rate, 2, &format, 256, blocks, &device);
    if (result == FMOD_OK) require(format == FMOD_SOUND_FORMAT_PCM16);
    return result;
}

static void emptyBuffer(FMOD::Output* output) {
    void *first = nullptr, *second = nullptr;
    unsigned firstLength = 0, secondLength = 0;
    require(output->mDescription.lock(output, 0, 4, &first, &second, &firstLength, &secondLength) == FMOD_ERR_INVALID_PARAM);
}

int main() {
    require(FMOD_Memory_Initialize(nullptr, 0, allocate, reallocate, deallocate) == FMOD_OK);
    auto* factory = FMOD_Object_Calloc(FMOD::PluginFactory);
    require(factory && FMOD_OS_Output_Register(factory) == FMOD_OK);
    FMOD::FMOD_OUTPUT_DESCRIPTION_EX* description = nullptr;
    require(factory->getOutput(0, &description) == FMOD_OK);
    require(description->mType == FMOD_OUTPUTTYPE_ALSA && description->polling);
    require(description->mSize == sizeof(FMOD::OutputALSA));
    const int registered = live;

    // Allocation failure must return an error without placement-new on null.
    FMOD::Output* absent = nullptr;
    failAllocation = true;
    require(factory->createOutput(description, &absent) == FMOD_ERR_MEMORY && !absent);
    require(live == registered);

    // Actual init callback: invalid inputs, device-open, parameters, buffer and
    // thread-create failures. Close is safe twice, including before init.
    for (int test = 0; test < 7; ++test) {
        auto* output = create(factory, description);
        emptyBuffer(output);
        require(output->mDescription.close(output) == FMOD_OK);
        const int opened = closes, started = creates, joined = joins;
        failure = test == 4 ? Parameters : test == 5 ? Buffer : test == 6 ? Writer : None;
        auto result = initialize(output, test == 3 ? "rov_nonexistent_pcm_186" : "null",
                                 test == 1 ? 0 : 48000, test == 2 ? 0 : 4);
        // test 0 covers close after successful init without mixer startup.
        if (test == 0) {
            require(result == FMOD_OK);
        } else {
            require(result == (test <= 2 || test == 4 ? FMOD_ERR_OUTPUT_FORMAT :
                               test == 5 ? FMOD_ERR_MEMORY : FMOD_ERR_OUTPUT_INIT));
            require(creates == started && joins == joined);
            require(closes == opened + (test >= 4));
        }
        require(output->mDescription.close(output) == FMOD_OK);
        require(output->mDescription.close(output) == FMOD_OK);
        emptyBuffer(output);
        require(output->release() == FMOD_OK && live == registered);
    }
    failure = None;

    for (int cycle = 0; cycle < 3; ++cycle) {
        auto* output = create(factory, description);
        require(initialize(output, "null") == FMOD_OK);
        require(typeid(*output) == typeid(FMOD::OutputALSA));
        unsigned position = 0;
        for (int wait = 0; wait < 2000 && !position; ++wait) {
            require(output->mDescription.getposition(output, &position) == FMOD_OK);
            usleep(1000);
        }
        require(position >= 256 && writes > 0);
        void *first = nullptr, *second = nullptr;
        unsigned firstLength = 0, secondLength = 0;
        require(output->mDescription.lock(output, 4096 - 4, 8, &first, &second, &firstLength, &secondLength) == FMOD_OK);
        require(first && second && firstLength == 4 && secondLength == 4);
        require(output->mDescription.close(output) == FMOD_OK);
        require(creates == joins);
        emptyBuffer(output);
        require(output->release() == FMOD_OK && live == registered);
    }
    require(drops == creates && creates == joins);
    auto* uninitialized = create(factory, description);
    require(uninitialized->release() == FMOD_OK && live == registered);
    require(factory->release() == FMOD_OK && live == 0);

    // Exercise both real SystemI output-replacement routes and failed-init
    // cleanup, with no successful fake device or mixer initialization.
    FMOD::System* system = nullptr;
    require(FMOD::System_Create(&system) == FMOD_OK);
    require(system->setOutput(FMOD_OUTPUTTYPE_ALSA) == FMOD_OK);
    require(system->setOutput(FMOD_OUTPUTTYPE_NOSOUND) == FMOD_OK);
    require(system->setOutput(FMOD_OUTPUTTYPE_ALSA) == FMOD_OK);
    int handle = -1;
    require(system->getOutputByPlugin(&handle) == FMOD_OK);
    require(system->setOutputByPlugin(handle) == FMOD_OK);
    const char* missing = "rov_nonexistent_pcm_186";
    require(system->init(8, FMOD_INIT_NORMAL, &missing) == FMOD_ERR_OUTPUT_INIT);
    require(system->close() == FMOD_OK);
    require(system->release() == FMOD_OK && live == 0);
    require(FMOD_Memory_Initialize(nullptr, 0, nullptr, nullptr, nullptr) == FMOD_OK);
    std::puts("native-alsa-output=passed: registered concrete type, original callbacks, real null PCM/writer/join, init failures, selection/release and balanced FMOD ownership");
}
