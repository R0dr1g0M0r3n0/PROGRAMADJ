#include "audioforge/engine.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static int check_condition(int condition, const char* description) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        return 0;
    }

    return 1;
}

int main(void) {
    const AudioForgeVersion version = audioforge_engine_get_version();
    if (!check_condition(version.major == AUDIOFORGE_API_VERSION_MAJOR,
                         "major version matches the API version")) {
        return EXIT_FAILURE;
    }

    if (!check_condition(version.minor == AUDIOFORGE_API_VERSION_MINOR,
                         "minor version matches the API version") ||
        !check_condition(version.patch == AUDIOFORGE_API_VERSION_PATCH,
                         "patch version matches the API version") ||
        !check_condition(
            audioforge_engine_create(NULL) == AUDIOFORGE_STATUS_INVALID_ARGUMENT,
            "create rejects a null output slot")) {
        return EXIT_FAILURE;
    }

    AudioForgeEngine* engine = NULL;
    if (!check_condition(audioforge_engine_create(&engine) == AUDIOFORGE_STATUS_OK,
                         "create succeeds with a valid output slot") ||
        !check_condition(engine != NULL, "create returns an owned engine handle")) {
        return EXIT_FAILURE;
    }

    audioforge_engine_destroy(engine);
    audioforge_engine_destroy(NULL);

    return 0;
}
