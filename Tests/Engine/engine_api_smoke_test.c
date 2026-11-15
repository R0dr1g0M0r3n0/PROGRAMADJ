#include "audioforge/engine.h"

#include <assert.h>
#include <stddef.h>

int main(void) {
    const AudioForgeVersion version = audioforge_engine_get_version();
    assert(version.major == AUDIOFORGE_API_VERSION_MAJOR);
    assert(version.minor == AUDIOFORGE_API_VERSION_MINOR);
    assert(version.patch == AUDIOFORGE_API_VERSION_PATCH);

    assert(audioforge_engine_create(NULL) == AUDIOFORGE_STATUS_INVALID_ARGUMENT);

    AudioForgeEngine* engine = NULL;
    assert(audioforge_engine_create(&engine) == AUDIOFORGE_STATUS_OK);
    assert(engine != NULL);

    audioforge_engine_destroy(engine);
    audioforge_engine_destroy(NULL);

    return 0;
}
