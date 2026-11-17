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

void reset_analysis_result(AudioForgeAnalysisResult* result,
                           AudioForgeStatus status) {
    const uint32_t declared_size = result->struct_size;
    std::memset(result, 0, sizeof(*result));
    result->struct_size = declared_size;
    result->api_version = AudioForgeVersion AUDIOFORGE_API_VERSION_INIT;
    result->status = status;
}

bool is_power_of_two(uint32_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

bool ranges_overlap(const void* first, size_t first_size,
                    const void* second, size_t second_size) {
    if (first_size == 0 || second_size == 0) {
        return false;
    }
    const uintptr_t first_start = reinterpret_cast<uintptr_t>(first);
    const uintptr_t second_start = reinterpret_cast<uintptr_t>(second);
    if (first_start > std::numeric_limits<uintptr_t>::max() - first_size ||
        second_start > std::numeric_limits<uintptr_t>::max() - second_size) {
        return true;
    }
    return first_start < second_start + second_size &&
           second_start < first_start + first_size;
}

bool analysis_output_ranges_are_disjoint(
    const AudioForgeAudioBuffer* input,
    float* magnitudes, size_t magnitude_bytes,
    uint64_t* frame_starts, size_t frame_start_bytes,
    float* frequencies, size_t frequency_bytes) {
    const uint64_t input_sample_count = input->frame_count * input->channel_count;
    const size_t input_bytes =
        static_cast<size_t>(input_sample_count) * sizeof(float);
    return !ranges_overlap(input->samples, input_bytes, magnitudes,
                           magnitude_bytes) &&
           !ranges_overlap(input->samples, input_bytes, frame_starts,
                           frame_start_bytes) &&
           !ranges_overlap(input->samples, input_bytes, frequencies,
                           frequency_bytes) &&
           !ranges_overlap(magnitudes, magnitude_bytes, frame_starts,
                           frame_start_bytes) &&
           !ranges_overlap(magnitudes, magnitude_bytes, frequencies,
                           frequency_bytes) &&
           !ranges_overlap(frame_starts, frame_start_bytes, frequencies,
                           frequency_bytes);
}

}  // namespace

extern "C" AudioForgeStatus audioforge_engine_analyze(
    const AudioForgeAnalysisRequest* request,
    AudioForgeAnalysisResult* result) {
    if (result == nullptr || result->struct_size < sizeof(*result)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    reset_analysis_result(result, AUDIOFORGE_STATUS_INVALID_ARGUMENT);

    if (request == nullptr || request->struct_size < sizeof(*request)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    if (request->api_version.major != AUDIOFORGE_API_VERSION_MAJOR ||
        request->api_version.minor != AUDIOFORGE_API_VERSION_MINOR ||
        request->api_version.patch != AUDIOFORGE_API_VERSION_PATCH) {
        result->status = AUDIOFORGE_STATUS_VERSION_MISMATCH;
        return AUDIOFORGE_STATUS_VERSION_MISMATCH;
    }
    if (request->reserved != 0 || request->input == nullptr ||
        audioforge_audio_buffer_validate(request->input) !=
            AUDIOFORGE_STATUS_OK ||
        request->channel_index >= request->input->channel_count ||
        request->fft_size < 2 ||
        request->fft_size > AUDIOFORGE_ANALYSIS_MAX_FFT_SIZE ||
        !is_power_of_two(request->fft_size) || request->hop_size == 0) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    const uint64_t frame_count =
        request->input->frame_count < request->fft_size
            ? 0
            : 1 + (request->input->frame_count - request->fft_size) /
                      request->hop_size;
    const uint64_t bin_count = request->fft_size / 2 + 1;
    if (frame_count != 0 && bin_count >
                                std::numeric_limits<uint64_t>::max() /
                                    frame_count) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    const uint64_t magnitude_count = frame_count * bin_count;
    if (magnitude_count > std::numeric_limits<uint64_t>::max() /
                              request->fft_size ||
        magnitude_count * request->fft_size >
            AUDIOFORGE_ANALYSIS_MAX_DFT_OPERATIONS) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    if (request->magnitude_capacity < magnitude_count ||
        request->frame_start_capacity < frame_count ||
        request->frequency_capacity < bin_count ||
        (magnitude_count != 0 && request->output_magnitudes == nullptr) ||
        (frame_count != 0 && request->output_frame_starts == nullptr) ||
        request->output_frequencies_hz == nullptr ||
        magnitude_count > std::numeric_limits<size_t>::max() / sizeof(float) ||
        frame_count > std::numeric_limits<size_t>::max() / sizeof(uint64_t) ||
        bin_count > std::numeric_limits<size_t>::max() / sizeof(float)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    const size_t magnitude_bytes =
        static_cast<size_t>(magnitude_count) * sizeof(float);
    const size_t frame_start_bytes =
        static_cast<size_t>(frame_count) * sizeof(uint64_t);
    const size_t frequency_bytes = static_cast<size_t>(bin_count) * sizeof(float);
    if (!analysis_output_ranges_are_disjoint(
            request->input, request->output_magnitudes, magnitude_bytes,
            request->output_frame_starts, frame_start_bytes,
            request->output_frequencies_hz, frequency_bytes)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }

    for (uint64_t bin = 0; bin < bin_count; ++bin) {
        request->output_frequencies_hz[bin] =
            static_cast<float>(static_cast<double>(bin) *
                               request->input->sample_rate / request->fft_size);
    }

    const double pi = std::acos(-1.0);
    for (uint64_t frame = 0; frame < frame_count; ++frame) {
        const uint64_t frame_start = frame * request->hop_size;
        request->output_frame_starts[frame] = frame_start;
        for (uint64_t bin = 0; bin < bin_count; ++bin) {
            double real = 0.0;
            double imaginary = 0.0;
            for (uint32_t sample_index = 0;
                 sample_index < request->fft_size; ++sample_index) {
                const uint64_t input_frame = frame_start + sample_index;
                const uint64_t sample_offset =
                    input_frame * request->input->channel_count +
                    request->channel_index;
                const double angle = 2.0 * pi * bin * sample_index /
                                     request->fft_size;
                const double sample = request->input->samples[sample_offset];
                real += sample * std::cos(angle);
                imaginary -= sample * std::sin(angle);
            }
            const uint64_t output_index = frame * bin_count + bin;
            request->output_magnitudes[output_index] =
                static_cast<float>(std::sqrt(real * real + imaginary * imaginary) /
                                   request->fft_size);
        }
    }

    result->status = AUDIOFORGE_STATUS_OK;
    result->frame_count = frame_count;
    result->bin_count = bin_count;
    result->magnitudes_written = magnitude_count;
    return AUDIOFORGE_STATUS_OK;
}

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
