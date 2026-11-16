#ifndef AUDIOFORGE_ENGINE_H
#define AUDIOFORGE_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIOFORGE_API_VERSION_MAJOR 1
#define AUDIOFORGE_API_VERSION_MINOR 0
#define AUDIOFORGE_API_VERSION_PATCH 0

#define AUDIOFORGE_API_VERSION_INIT \
    { AUDIOFORGE_API_VERSION_MAJOR, AUDIOFORGE_API_VERSION_MINOR, AUDIOFORGE_API_VERSION_PATCH }

typedef struct AudioForgeEngine AudioForgeEngine;

typedef struct AudioForgeVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} AudioForgeVersion;

typedef enum AudioForgeStatus {
    AUDIOFORGE_STATUS_OK = 0,
    AUDIOFORGE_STATUS_INVALID_ARGUMENT = 1,
    AUDIOFORGE_STATUS_OUT_OF_MEMORY = 2,
    AUDIOFORGE_STATUS_VERSION_MISMATCH = 3,
    AUDIOFORGE_STATUS_UNSUPPORTED = 4
} AudioForgeStatus;

typedef enum AudioForgeErrorCode {
    AUDIOFORGE_ERROR_NONE = 0,
    AUDIOFORGE_ERROR_INVALID_ARGUMENT = 1,
    AUDIOFORGE_ERROR_VERSION_MISMATCH = 2,
    AUDIOFORGE_ERROR_UNSUPPORTED_PROCESSING = 3,
    AUDIOFORGE_ERROR_INVALID_CANCELLATION_TOKEN = 4,
    AUDIOFORGE_ERROR_INVALID_TIME_TRANSFORM = 5
} AudioForgeErrorCode;

typedef struct AudioForgeCancellationToken {
    uint32_t struct_size;
    uint32_t state;
} AudioForgeCancellationToken;

enum {
    AUDIOFORGE_CANCELLATION_PENDING = 0,
    AUDIOFORGE_CANCELLATION_CANCELLED = 1
};

#define AUDIOFORGE_CANCELLATION_TOKEN_INIT \
    { (uint32_t)sizeof(AudioForgeCancellationToken), AUDIOFORGE_CANCELLATION_PENDING }

typedef struct AudioForgeTimeTransform {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t source_start_frame;
    uint64_t source_frame_count;
    uint64_t output_start_frame;
    uint64_t output_frame_count;
} AudioForgeTimeTransform;

#define AUDIOFORGE_TIME_TRANSFORM_STRUCT_SIZE \
    ((uint32_t)sizeof(AudioForgeTimeTransform))

typedef struct AudioForgeProgress {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t completed_units;
    uint64_t total_units;
} AudioForgeProgress;

typedef void (*AudioForgeProgressCallback)(const AudioForgeProgress* progress,
                                           void* context);

/* Declarative future audio-thread callback only: this API does not invoke it.
 * Implementations must not allocate, lock, block, log, or throw exceptions. */
typedef void (*AudioForgeRealtimeCallback)(const float* input,
                                           float* output,
                                           uint64_t frame_count,
                                           uint32_t channel_count,
                                           void* context);

typedef struct AudioForgeInputBuffer {
    const float* samples;
    uint64_t sample_count;
} AudioForgeInputBuffer;

typedef struct AudioForgeOutputBuffer {
    float* samples;
    uint64_t capacity_samples;
} AudioForgeOutputBuffer;

typedef struct AudioForgeProcessRequest {
    uint32_t struct_size;
    AudioForgeVersion api_version;
    AudioForgeInputBuffer input;
    AudioForgeOutputBuffer output;
    const AudioForgeCancellationToken* cancellation;
    const AudioForgeTimeTransform* time_transform;
    /* Future synchronous implementations invoke this on the caller's
     * control thread only, never on an audio thread. */
    AudioForgeProgressCallback progress_callback;
    void* progress_context;
} AudioForgeProcessRequest;

#define AUDIOFORGE_PROCESS_REQUEST_INIT \
    { (uint32_t)sizeof(AudioForgeProcessRequest), AUDIOFORGE_API_VERSION_INIT, \
      { NULL, 0 }, { NULL, 0 }, NULL, NULL, NULL, NULL }

typedef struct AudioForgeProcessResult {
    uint32_t struct_size;
    AudioForgeVersion api_version;
    AudioForgeStatus status;
    uint32_t reserved;
    uint64_t frames_processed;
    uint64_t output_samples_written;
} AudioForgeProcessResult;

#define AUDIOFORGE_PROCESS_RESULT_INIT \
    { (uint32_t)sizeof(AudioForgeProcessResult), AUDIOFORGE_API_VERSION_INIT, \
      AUDIOFORGE_STATUS_OK, 0, 0, 0 }

typedef struct AudioForgeError {
    uint32_t struct_size;
    AudioForgeErrorCode code;
    uint32_t reserved;
} AudioForgeError;

#define AUDIOFORGE_ERROR_INIT \
    { (uint32_t)sizeof(AudioForgeError), AUDIOFORGE_ERROR_NONE, 0 }

AudioForgeVersion audioforge_engine_get_version(void);

/* On success, stores a caller-owned engine handle in out_engine. On failure,
 * stores NULL there when out_engine is non-NULL. The caller releases a handle
 * exactly once with audioforge_engine_destroy. */
AudioForgeStatus audioforge_engine_create(AudioForgeEngine** out_engine);

/* Releases a handle created by audioforge_engine_create. NULL is accepted. */
void audioforge_engine_destroy(AudioForgeEngine* engine);

/* Buffers and callback context are borrowed for this synchronous call only;
 * no pointer is retained. The caller asserts output capacity is writable.
 * On failure the entire supplied output capacity is zeroed when its byte size
 * is representable. Result and error records must declare at least their
 * current struct sizes; undersized records are rejected without modifying
 * either record. Valid records are written only through the current struct
 * prefix. This contract currently validates and returns UNSUPPORTED; it does
 * not perform audio processing or invoke either callback. */
AudioForgeStatus audioforge_engine_process(
    const AudioForgeProcessRequest* request,
    AudioForgeProcessResult* result,
    AudioForgeError* error);

/* Token state must be initialized with AUDIOFORGE_CANCELLATION_TOKEN_INIT.
 * Cancellation may be requested from another thread; repeated cancellation
 * is idempotent. The token remains caller-owned and must outlive any call that
 * references it. */
AudioForgeStatus audioforge_cancellation_token_validate(
    const AudioForgeCancellationToken* token);
AudioForgeStatus audioforge_cancellation_token_cancel(
    AudioForgeCancellationToken* token);
AudioForgeStatus audioforge_cancellation_token_is_cancelled(
    const AudioForgeCancellationToken* token,
    int32_t* is_cancelled);

/* The source and output frame fields are absolute frame-domain coordinates.
 * The source interval is half-open [start, start + count); the corresponding
 * output offset is floor(source_offset * output_count / source_count). */
AudioForgeStatus audioforge_time_transform_map_source_frame(
    const AudioForgeTimeTransform* transform,
    uint64_t source_frame,
    uint64_t* output_frame);


typedef struct AudioForgeAudioBuffer {
    uint32_t struct_size;
    uint32_t channel_count;
    uint32_t sample_rate;
    uint32_t reserved;
    uint64_t frame_count;
    const float* samples;
} AudioForgeAudioBuffer;

#define AUDIOFORGE_AUDIO_BUFFER_INIT \
    { (uint32_t)sizeof(AudioForgeAudioBuffer), 0, 0, 0, 0, NULL }

typedef struct AudioForgeAudioBufferOutput {
    uint32_t struct_size;
    uint32_t channel_count;
    uint32_t sample_rate;
    uint32_t reserved;
    uint64_t frame_capacity;
    uint64_t frame_count;
    float* samples;
} AudioForgeAudioBufferOutput;

#define AUDIOFORGE_AUDIO_BUFFER_OUTPUT_INIT \
    { (uint32_t)sizeof(AudioForgeAudioBufferOutput), 0, 0, 0, 0, 0, NULL }

/* In-memory import/export contract for interleaved float samples. The caller
 * owns all sample storage; these primitives never allocate or retain pointers.
 * Supported sample rates are 8 kHz through 384 kHz and channel counts are
 * limited to 32. A zero-frame buffer may have a NULL sample pointer. */
static inline AudioForgeStatus audioforge_audio_buffer_validate(
    const AudioForgeAudioBuffer* buffer) {
    if (buffer == NULL || buffer->struct_size < sizeof(*buffer) ||
        buffer->reserved != 0 || buffer->channel_count == 0 ||
        buffer->channel_count > 32 || buffer->sample_rate < 8000 ||
        buffer->sample_rate > 384000 ||
        (buffer->frame_count != 0 && buffer->samples == NULL) ||
        (buffer->frame_count != 0 &&
         buffer->frame_count > UINT64_MAX / buffer->channel_count)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    const uint64_t sample_count =
        buffer->frame_count * buffer->channel_count;
    if (sample_count > SIZE_MAX / sizeof(float)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    return AUDIOFORGE_STATUS_OK;
}

static inline AudioForgeStatus audioforge_audio_buffer_import(
    const float* samples,
    uint64_t frame_count,
    uint32_t channel_count,
    uint32_t sample_rate,
    AudioForgeAudioBuffer* out_buffer) {
    if (out_buffer == NULL || out_buffer->struct_size < sizeof(*out_buffer)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    if (frame_count != 0 && samples == NULL) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    AudioForgeAudioBuffer candidate = AUDIOFORGE_AUDIO_BUFFER_INIT;
    candidate.channel_count = channel_count;
    candidate.sample_rate = sample_rate;
    candidate.frame_count = frame_count;
    candidate.samples = samples;
    if (audioforge_audio_buffer_validate(&candidate) != AUDIOFORGE_STATUS_OK) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    *out_buffer = candidate;
    return AUDIOFORGE_STATUS_OK;
}

static inline AudioForgeStatus audioforge_audio_buffer_export(
    const AudioForgeAudioBuffer* source,
    AudioForgeAudioBufferOutput* destination) {
    if (destination != NULL) {
        destination->frame_count = 0;
    }
    if (audioforge_audio_buffer_validate(source) != AUDIOFORGE_STATUS_OK ||
        destination == NULL ||
        destination->struct_size < sizeof(*destination) ||
        destination->reserved != 0 || destination->channel_count == 0 ||
        destination->channel_count > 32 || destination->sample_rate < 8000 ||
        destination->sample_rate > 384000 ||
        destination->channel_count != source->channel_count ||
        destination->sample_rate != source->sample_rate ||
        destination->frame_capacity < source->frame_count ||
        (source->frame_count != 0 && destination->samples == NULL) ||
        source->frame_count * source->channel_count >
            SIZE_MAX / sizeof(float)) {
        return AUDIOFORGE_STATUS_INVALID_ARGUMENT;
    }
    const size_t byte_count = (size_t)(source->frame_count *
                                       source->channel_count) * sizeof(float);
    if (byte_count != 0) {
        memcpy(destination->samples, source->samples, byte_count);
    }
    destination->frame_count = source->frame_count;
    return AUDIOFORGE_STATUS_OK;
}

AudioForgeStatus audioforge_gain_apply(
    const AudioForgeAudioBuffer* input,
    float gain,
    AudioForgeAudioBufferOutput* output);

#ifdef __cplusplus
}
#endif

#endif
