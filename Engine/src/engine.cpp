#include "audioforge/engine.h"

#include <new>

struct AudioForgeEngine {
    // Engine state is intentionally empty until a later task defines behavior.
};

extern "C" AudioForgeVersion audioforge_engine_get_version(void) {
    return {
        AUDIOFORGE_API_VERSION_MAJOR,
        AUDIOFORGE_API_VERSION_MINOR,
        AUDIOFORGE_API_VERSION_PATCH,
    };
}

extern "C" AudioForgeStatus audioforge_engine_create(
    AudioForgeEngine** out_engine) {
    if (out_engine == nullptr) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    *out_engine = nullptr;
    AudioForgeEngine* engine = new (std::nothrow) AudioForgeEngine{};
    if (engine == nullptr) {
        return AUDIOFORGE_STATUS_OUT_OF_MEMORY;
    }

    *out_engine = engine;
    return AUDIOFORGE_STATUS_OK;
}

extern "C" void audioforge_engine_destroy(AudioForgeEngine* engine) {
    delete engine;
}
