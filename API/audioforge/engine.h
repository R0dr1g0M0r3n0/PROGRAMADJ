#ifndef AUDIOFORGE_ENGINE_H
#define AUDIOFORGE_ENGINE_H

#include <stdint.h>
#include <stddef.h>

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

AudioForgeVersion audioforge_engine_get_version(void);

/* On success, stores a caller-owned engine handle in out_engine. On failure,
 * stores NULL there when out_engine is non-NULL. The caller releases a handle
 * exactly once with audioforge_engine_destroy. */
AudioForgeStatus audioforge_engine_create(AudioForgeEngine** out_engine);

/* Releases a handle created by audioforge_engine_create. NULL is accepted. */
void audioforge_engine_destroy(AudioForgeEngine* engine);

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

#ifdef __cplusplus
}
#endif

#endif
