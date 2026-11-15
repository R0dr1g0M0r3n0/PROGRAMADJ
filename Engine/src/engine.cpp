#include "audioforge/engine.h"

#include <cstring>
#include <limits>
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

namespace {

AudioForgeStatus validate_time_transform(
    const AudioForgeTimeTransform* transform) {
    if (transform == nullptr ||
        transform->struct_size < sizeof(AudioForgeTimeTransform) ||
        transform->reserved != 0 || transform->source_frame_count == 0 ||
        transform->output_frame_count == 0 ||
        transform->source_start_frame >
            std::numeric_limits<uint64_t>::max() - transform->source_frame_count ||
        transform->output_start_frame >
            std::numeric_limits<uint64_t>::max() - transform->output_frame_count) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    return AUDIOFORGE_STATUS_OK;
}

}  // namespace

extern "C" AudioForgeStatus audioforge_cancellation_token_validate(
    const AudioForgeCancellationToken* token) {
    if (token == nullptr || token->struct_size < sizeof(*token)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    const uint32_t state = __atomic_load_n(&token->state, __ATOMIC_ACQUIRE);
    if (state != AUDIOFORGE_CANCELLATION_PENDING &&
        state != AUDIOFORGE_CANCELLATION_CANCELLED) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    return AUDIOFORGE_STATUS_OK;
}

extern "C" AudioForgeStatus audioforge_cancellation_token_cancel(
    AudioForgeCancellationToken* token) {
    if (audioforge_cancellation_token_validate(token) != AUDIOFORGE_STATUS_OK) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    uint32_t expected = AUDIOFORGE_CANCELLATION_PENDING;
    __atomic_compare_exchange_n(&token->state, &expected,
                                AUDIOFORGE_CANCELLATION_CANCELLED, false,
                                __ATOMIC_RELEASE, __ATOMIC_RELAXED);
    return AUDIOFORGE_STATUS_OK;
}

extern "C" AudioForgeStatus audioforge_cancellation_token_is_cancelled(
    const AudioForgeCancellationToken* token,
    int32_t* is_cancelled) {
    if (is_cancelled != nullptr) {
        *is_cancelled = 0;
    }
    if (is_cancelled == nullptr ||
        audioforge_cancellation_token_validate(token) != AUDIOFORGE_STATUS_OK) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    *is_cancelled = __atomic_load_n(&token->state, __ATOMIC_ACQUIRE) ==
                    AUDIOFORGE_CANCELLATION_CANCELLED;
    return AUDIOFORGE_STATUS_OK;
}

extern "C" AudioForgeStatus audioforge_time_transform_map_source_frame(
    const AudioForgeTimeTransform* transform,
    uint64_t source_frame,
    uint64_t* output_frame) {
    if (output_frame != nullptr) {
        *output_frame = 0;
    }
    if (output_frame == nullptr ||
        validate_time_transform(transform) != AUDIOFORGE_STATUS_OK ||
        source_frame < transform->source_start_frame ||
        source_frame - transform->source_start_frame >=
            transform->source_frame_count) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    const uint64_t offset = source_frame - transform->source_start_frame;
    if (offset != 0 &&
        transform->output_frame_count >
            std::numeric_limits<uint64_t>::max() / offset) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    const uint64_t output_offset =
        (offset * transform->output_frame_count) /
        transform->source_frame_count;
    if (transform->output_start_frame >
        std::numeric_limits<uint64_t>::max() - output_offset) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    *output_frame = transform->output_start_frame + output_offset;
    return AUDIOFORGE_STATUS_OK;
}
