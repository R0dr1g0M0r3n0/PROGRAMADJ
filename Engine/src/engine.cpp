#include "audioforge/engine.h"

#include <cmath>
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

void set_result(AudioForgeProcessResult* result, AudioForgeStatus status) {
    const uint32_t declared_size = result->struct_size;
    std::memset(result, 0, sizeof(*result));
    result->struct_size = declared_size;
    result->api_version = AudioForgeVersion AUDIOFORGE_API_VERSION_INIT;
    result->status = status;
}

void set_error(AudioForgeError* error, AudioForgeErrorCode code) {
    const uint32_t declared_size = error->struct_size;
    std::memset(error, 0, sizeof(*error));
    error->struct_size = declared_size;
    error->code = code;
}

bool has_writable_output_records(const AudioForgeProcessResult* result,
                                 const AudioForgeError* error) {
    return result != nullptr && error != nullptr &&
           result->struct_size >= sizeof(*result) &&
           error->struct_size >= sizeof(*error);
}

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

void clear_output(const AudioForgeProcessRequest* request) {
    if (request == nullptr ||
        request->struct_size < sizeof(AudioForgeProcessRequest) ||
        request->output.samples == nullptr ||
        request->output.capacity_samples >
            std::numeric_limits<size_t>::max() / sizeof(float)) {
        return;
    }
    std::memset(request->output.samples, 0,
                static_cast<size_t>(request->output.capacity_samples) *
                    sizeof(float));
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

extern "C" AudioForgeStatus audioforge_gain_apply(
    const AudioForgeAudioBuffer* input,
    float gain,
    AudioForgeAudioBufferOutput* output) {
    if (output != nullptr) {
        output->frame_count = 0;
    }

    if (audioforge_audio_buffer_validate(input) != AUDIOFORGE_STATUS_OK ||
        output == nullptr || output->struct_size < sizeof(*output) ||
        output->reserved != 0 || !std::isfinite(gain) ||
        output->channel_count != input->channel_count ||
        output->sample_rate != input->sample_rate ||
        output->frame_capacity < input->frame_count ||
        (input->frame_count != 0 && output->samples == nullptr)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    const size_t sample_count = static_cast<size_t>(
        input->frame_count * input->channel_count);
    for (size_t index = 0; index < sample_count; ++index) {
        output->samples[index] = input->samples[index] * gain;
    }
    output->frame_count = input->frame_count;
    return AUDIOFORGE_STATUS_OK;
}

extern "C" AudioForgeStatus audioforge_engine_process(
    const AudioForgeProcessRequest* request,
    AudioForgeProcessResult* result,
    AudioForgeError* error) {
    clear_output(request);

    if (!has_writable_output_records(result, error)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    set_result(result, AUDIOFORGE_STATUS_INVALID_ARGUMENT);
    set_error(error, AUDIOFORGE_ERROR_INVALID_ARGUMENT);

    if (request == nullptr ||
        request->struct_size < sizeof(AudioForgeProcessRequest)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    if (request->api_version.major != AUDIOFORGE_API_VERSION_MAJOR ||
        request->api_version.minor != AUDIOFORGE_API_VERSION_MINOR ||
        request->api_version.patch != AUDIOFORGE_API_VERSION_PATCH) {
        set_result(result, AUDIOFORGE_STATUS_VERSION_MISMATCH);
        set_error(error, AUDIOFORGE_ERROR_VERSION_MISMATCH);
        return AUDIOFORGE_STATUS_VERSION_MISMATCH;
    }

    if ((request->input.sample_count != 0 && request->input.samples == nullptr) ||
        (request->output.capacity_samples != 0 &&
         request->output.samples == nullptr) ||
        (request->cancellation != nullptr &&
         audioforge_cancellation_token_validate(request->cancellation) !=
             AUDIOFORGE_STATUS_OK)) {
        if (request->cancellation != nullptr &&
            audioforge_cancellation_token_validate(request->cancellation) !=
                AUDIOFORGE_STATUS_OK) {
            set_error(error, AUDIOFORGE_ERROR_INVALID_CANCELLATION_TOKEN);
        }
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    if (request->time_transform != nullptr &&
        validate_time_transform(request->time_transform) != AUDIOFORGE_STATUS_OK) {
        set_error(error, AUDIOFORGE_ERROR_INVALID_TIME_TRANSFORM);
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    set_result(result, AUDIOFORGE_STATUS_UNSUPPORTED);
    set_error(error, AUDIOFORGE_ERROR_UNSUPPORTED_PROCESSING);
    return AUDIOFORGE_STATUS_UNSUPPORTED;
}
