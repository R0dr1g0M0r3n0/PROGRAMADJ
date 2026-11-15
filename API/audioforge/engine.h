#ifndef AUDIOFORGE_ENGINE_H
#define AUDIOFORGE_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIOFORGE_API_VERSION_MAJOR 1
#define AUDIOFORGE_API_VERSION_MINOR 0
#define AUDIOFORGE_API_VERSION_PATCH 0

typedef struct AudioForgeEngine AudioForgeEngine;

typedef struct AudioForgeVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} AudioForgeVersion;

typedef enum AudioForgeStatus {
    AUDIOFORGE_STATUS_OK = 0,
    AUDIOFORGE_STATUS_INVALID_ARGUMENT = 1,
    AUDIOFORGE_STATUS_OUT_OF_MEMORY = 2
} AudioForgeStatus;

AudioForgeVersion audioforge_engine_get_version(void);

/* On success, stores a caller-owned engine handle in out_engine. On failure,
 * stores NULL there when out_engine is non-NULL. The caller releases a handle
 * exactly once with audioforge_engine_destroy. */
AudioForgeStatus audioforge_engine_create(AudioForgeEngine** out_engine);

/* Releases a handle created by audioforge_engine_create. NULL is accepted. */
void audioforge_engine_destroy(AudioForgeEngine* engine);

#ifdef __cplusplus
}
#endif

#endif
