#include "audioforge/engine.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    AudioForgeCancellationToken token = AUDIOFORGE_CANCELLATION_TOKEN_INIT;
    int32_t is_cancelled = 1;
    if (!check_condition(audioforge_cancellation_token_validate(&token) ==
                             AUDIOFORGE_STATUS_OK,
                         "pending cancellation token validates") ||
        !check_condition(audioforge_cancellation_token_is_cancelled(
                             &token, &is_cancelled) == AUDIOFORGE_STATUS_OK &&
                             is_cancelled == 0,
                         "pending cancellation token reports not cancelled") ||
        !check_condition(audioforge_cancellation_token_cancel(&token) ==
                             AUDIOFORGE_STATUS_OK,
                         "cancellation request succeeds") ||
        !check_condition(token.state == AUDIOFORGE_CANCELLATION_CANCELLED,
                         "cancellation state is observable") ||
        !check_condition(audioforge_cancellation_token_is_cancelled(
                             &token, &is_cancelled) == AUDIOFORGE_STATUS_OK &&
                             is_cancelled == 1,
                         "cancelled state is read through the atomic API") ||
        !check_condition(audioforge_cancellation_token_cancel(&token) ==
                             AUDIOFORGE_STATUS_OK,
                         "repeated cancellation is idempotent")) {
        return EXIT_FAILURE;
    }
    token.state = 99;
    if (!check_condition(audioforge_cancellation_token_validate(&token) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "invalid cancellation state is rejected")) {
        return EXIT_FAILURE;
    }

    const AudioForgeTimeTransform transform = {
        AUDIOFORGE_TIME_TRANSFORM_STRUCT_SIZE,
        0,
        0,
        100,
        200,
        50,
    };
    uint64_t mapped_frame = UINT64_MAX;
    if (!check_condition(audioforge_time_transform_map_source_frame(
                             &transform, 25, &mapped_frame) ==
                             AUDIOFORGE_STATUS_OK,
                         "source frame maps into the output domain") ||
        !check_condition(mapped_frame == 212,
                         "frame mapping is deterministic and integer based") ||
        !check_condition(audioforge_time_transform_map_source_frame(
                             &transform, 100, &mapped_frame) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "source frame at the exclusive end is rejected")) {
        return EXIT_FAILURE;
    }

    const AudioForgeTimeTransform overflow_transform = {
        AUDIOFORGE_TIME_TRANSFORM_STRUCT_SIZE,
        0,
        0,
        UINT64_MAX,
        0,
        UINT64_MAX,
    };
    if (!check_condition(audioforge_time_transform_map_source_frame(
                             &overflow_transform, 2, &mapped_frame) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "frame mapping rejects checked multiplication overflow") ||
        !check_condition(mapped_frame == 0,
                         "failed frame mapping clears its output slot")) {
        return EXIT_FAILURE;
    }

    return 0;
}
