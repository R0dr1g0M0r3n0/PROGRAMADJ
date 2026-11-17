#include "audioforge/engine.h"
#include "../../TestFixtures/audio_buffer_fixture_v1.h"

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

    /* Versioned processing contracts are declarative; processing is not yet
     * implemented and must fail deterministically without leaving output. */
    float input[2] = {1.0f, -1.0f};
    float output[4] = {7.0f, 7.0f, 7.0f, 7.0f};
    AudioForgeProcessRequest request = AUDIOFORGE_PROCESS_REQUEST_INIT;
    request.input.samples = input;
    request.input.sample_count = 2;
    request.output.samples = output;
    request.output.capacity_samples = 4;
    AudioForgeProcessResult result = AUDIOFORGE_PROCESS_RESULT_INIT;
    AudioForgeError error = AUDIOFORGE_ERROR_INIT;

    if (!check_condition(
            audioforge_engine_process(&request, &result, &error) ==
                AUDIOFORGE_STATUS_UNSUPPORTED,
            "processing reports unsupported until behavior is implemented") ||
        !check_condition(result.status == AUDIOFORGE_STATUS_UNSUPPORTED,
                         "result carries explicit unsupported status") ||
        !check_condition(error.code == AUDIOFORGE_ERROR_UNSUPPORTED_PROCESSING,
                         "error identifies unsupported processing") ||
        !check_condition(output[0] == 0.0f && output[1] == 0.0f &&
                             output[2] == 0.0f && output[3] == 0.0f,
                         "borrowed output is cleared on failure")) {
        return EXIT_FAILURE;
    }

    output[0] = output[1] = 9.0f;
    request.api_version.major += 1;
    if (!check_condition(
            audioforge_engine_process(&request, &result, &error) ==
                AUDIOFORGE_STATUS_VERSION_MISMATCH,
            "unknown API versions are rejected") ||
        !check_condition(output[0] == 0.0f && output[1] == 0.0f,
                         "version errors clear caller output")) {
        return EXIT_FAILURE;
    }

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

    request.api_version = (AudioForgeVersion)AUDIOFORGE_API_VERSION_INIT;
    request.cancellation = &token;
    output[0] = output[1] = 4.0f;
    if (!check_condition(audioforge_engine_process(&request, &result, &error) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "processing rejects an invalid cancellation token") ||
        !check_condition(error.code ==
                             AUDIOFORGE_ERROR_INVALID_CANCELLATION_TOKEN,
                         "invalid cancellation has a structured error") ||
        !check_condition(output[0] == 0.0f && output[1] == 0.0f,
                         "invalid-token failure clears output")) {
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

    struct SmallVersionedResult {
        uint32_t struct_size;
        unsigned char guard[64];
    } small_result = {sizeof(uint32_t), {0}};
    struct SmallVersionedError {
        uint32_t struct_size;
        unsigned char guard[64];
    } small_error = {sizeof(uint32_t), {0}};
    memset(small_result.guard, 0xa5, sizeof(small_result.guard));
    memset(small_error.guard, 0x5a, sizeof(small_error.guard));
    request.cancellation = NULL;
    request.time_transform = NULL;
    output[0] = output[1] = 3.0f;
    if (!check_condition(
            audioforge_engine_process(
                &request, (AudioForgeProcessResult*)&small_result,
                (AudioForgeError*)&small_error) ==
                AUDIOFORGE_STATUS_INVALID_ARGUMENT,
            "undersized versioned result and error records are rejected") ||
        !check_condition(small_result.struct_size == sizeof(uint32_t) &&
                             small_error.struct_size == sizeof(uint32_t),
                         "undersized record size headers remain unchanged") ||
        !check_condition(output[0] == 0.0f && output[1] == 0.0f,
                         "undersized metadata failure still clears caller output")) {
        return EXIT_FAILURE;
    }
    for (size_t index = 0; index < sizeof(small_result.guard); ++index) {
        if (!check_condition(small_result.guard[index] == 0xa5,
                             "result writes stay inside declared size") ||
            !check_condition(small_error.guard[index] == 0x5a,
                             "error writes stay inside declared size")) {
            return EXIT_FAILURE;
        }
    }

    AudioForgeAudioBuffer source = AUDIOFORGE_AUDIO_BUFFER_INIT;
    if (!check_condition(audioforge_audio_buffer_import(
                             AUDIOFORGE_FIXTURE_SAMPLES,
                             AUDIOFORGE_FIXTURE_FRAME_COUNT,
                             AUDIOFORGE_FIXTURE_CHANNEL_COUNT,
                             AUDIOFORGE_FIXTURE_SAMPLE_RATE,
                             &source) ==
                             AUDIOFORGE_STATUS_OK,
                         "audio import accepts a valid interleaved buffer") ||
        !check_condition(source.frame_count == AUDIOFORGE_FIXTURE_FRAME_COUNT &&
                             source.channel_count == AUDIOFORGE_FIXTURE_CHANNEL_COUNT &&
                             source.sample_rate == AUDIOFORGE_FIXTURE_SAMPLE_RATE,
                         "audio import records frame format") ||
        !check_condition(audioforge_audio_buffer_validate(&source) ==
                             AUDIOFORGE_STATUS_OK,
                         "valid audio buffer passes format validation")) {
        return EXIT_FAILURE;
    }

    float round_trip_samples[AUDIOFORGE_FIXTURE_SAMPLE_COUNT] = {0.0f};
    AudioForgeAudioBufferOutput destination =
        AUDIOFORGE_AUDIO_BUFFER_OUTPUT_INIT;
    destination.samples = round_trip_samples;
    destination.frame_capacity = AUDIOFORGE_FIXTURE_FRAME_COUNT;
    destination.channel_count = AUDIOFORGE_FIXTURE_CHANNEL_COUNT;
    destination.sample_rate = AUDIOFORGE_FIXTURE_SAMPLE_RATE;
    if (!check_condition(audioforge_audio_buffer_export(
                             &source, &destination) == AUDIOFORGE_STATUS_OK,
                         "audio export accepts matching capacity and format") ||
        !check_condition(destination.frame_count == AUDIOFORGE_FIXTURE_FRAME_COUNT,
                         "audio export reports copied frames") ||
        !check_condition(memcmp(AUDIOFORGE_FIXTURE_SAMPLES, round_trip_samples,
                                sizeof(AUDIOFORGE_FIXTURE_SAMPLES)) == 0,
                         "synthetic samples round-trip exactly")) {
        return EXIT_FAILURE;
    }

    float guarded_output[6] = {8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f};
    destination.samples = guarded_output;
    destination.frame_capacity = 2;
    destination.frame_count = 99;
    if (!check_condition(audioforge_audio_buffer_export(
                             &source, &destination) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "audio export rejects insufficient capacity") ||
        !check_condition(guarded_output[0] == 8.0f && guarded_output[5] == 8.0f,
                         "failed export does not modify destination samples") ||
        !check_condition(destination.frame_count == 0,
                         "failed export clears destination frame count")) {
        return EXIT_FAILURE;
    }

    AudioForgeAudioBuffer invalid = AUDIOFORGE_AUDIO_BUFFER_INIT;
    if (!check_condition(audioforge_audio_buffer_import(
                             AUDIOFORGE_FIXTURE_SAMPLES,
                             AUDIOFORGE_FIXTURE_FRAME_COUNT, 0,
                             AUDIOFORGE_FIXTURE_SAMPLE_RATE, &invalid) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "audio import rejects zero channels") ||
        !check_condition(audioforge_audio_buffer_import(
                             NULL, AUDIOFORGE_FIXTURE_FRAME_COUNT,
                             AUDIOFORGE_FIXTURE_CHANNEL_COUNT,
                             AUDIOFORGE_FIXTURE_SAMPLE_RATE, &invalid) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "audio import rejects missing samples") ||
        !check_condition(audioforge_audio_buffer_validate(NULL) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "audio validation rejects a null buffer")) {
        return EXIT_FAILURE;
    }

    /* Calibrated analysis uses a caller-owned interleaved buffer and writes
     * only caller-owned analysis arrays. */
    float tone_samples[32];
    float original_tone_samples[sizeof(tone_samples) / sizeof(tone_samples[0])];
    const float tone_period[8] = {
        0.0f, 0.70710678f, 1.0f, 0.70710678f,
        0.0f, -0.70710678f, -1.0f, -0.70710678f,
    };
    for (size_t frame = 0; frame < 16; ++frame) {
        const float sample = tone_period[frame % 8];
        tone_samples[frame * 2] = sample;
        tone_samples[frame * 2 + 1] = -sample;
    }
    memcpy(original_tone_samples, tone_samples, sizeof(tone_samples));
    AudioForgeAudioBuffer tone = AUDIOFORGE_AUDIO_BUFFER_INIT;
    tone.channel_count = 2;
    tone.sample_rate = 8000;
    tone.frame_count = 16;
    tone.samples = tone_samples;

    float magnitudes[21];
    uint64_t frame_starts[3];
    float frequencies[5];
    for (size_t index = 0; index < sizeof(magnitudes) / sizeof(magnitudes[0]); ++index) {
        magnitudes[index] = -1.0f;
    }
    for (size_t index = 0; index < 3; ++index) {
        frame_starts[index] = UINT64_MAX;
    }
    for (size_t index = 0; index < 5; ++index) {
        frequencies[index] = -1.0f;
    }
    AudioForgeAnalysisRequest analysis_request = AUDIOFORGE_ANALYSIS_REQUEST_INIT;
    analysis_request.input = &tone;
    analysis_request.channel_index = 0;
    analysis_request.fft_size = 8;
    analysis_request.hop_size = 4;
    analysis_request.output_magnitudes = magnitudes;
    analysis_request.magnitude_capacity = 21;
    analysis_request.output_frame_starts = frame_starts;
    analysis_request.frame_start_capacity = 3;
    analysis_request.output_frequencies_hz = frequencies;
    analysis_request.frequency_capacity = 5;
    AudioForgeAnalysisResult analysis_result = AUDIOFORGE_ANALYSIS_RESULT_INIT;

    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                    &analysis_result) ==
                             AUDIOFORGE_STATUS_OK,
                         "calibrated analysis accepts valid DFT settings") ||
        !check_condition(analysis_result.status == AUDIOFORGE_STATUS_OK &&
                             analysis_result.frame_count == 3 &&
                             analysis_result.bin_count == 5,
                         "analysis reports complete frame and bin counts") ||
        !check_condition(frame_starts[0] == 0 && frame_starts[1] == 4 &&
                             frame_starts[2] == 8,
                         "analysis reports frame starts in input-frame coordinates") ||
        !check_condition(frequencies[0] == 0.0f && frequencies[1] == 1000.0f &&
                             frequencies[2] == 2000.0f && frequencies[3] == 3000.0f &&
                             frequencies[4] == 4000.0f,
                         "analysis reports calibrated DFT bin frequencies") ||
        !check_condition(magnitudes[1] > 0.49f && magnitudes[1] < 0.51f,
                         "one-kilohertz tone appears in the expected normalized bin") ||
        !check_condition(memcmp(tone_samples, original_tone_samples,
                                sizeof(tone_samples)) == 0,
                         "analysis preserves input samples byte-for-byte")) {
        return EXIT_FAILURE;
    }

    magnitudes[0] = 17.0f;
    analysis_request.fft_size = 7;
    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                   &analysis_result) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "analysis rejects invalid FFT settings") ||
        !check_condition(magnitudes[0] == 17.0f,
                         "invalid analysis leaves output samples untouched")) {
        return EXIT_FAILURE;
    }

    analysis_request.fft_size = 8;
    analysis_request.hop_size = 0;
    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                   &analysis_result) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "analysis rejects a zero hop size") ||
        !check_condition(magnitudes[0] == 17.0f,
                         "invalid hop leaves output samples untouched")) {
        return EXIT_FAILURE;
    }
    analysis_request.hop_size = 4;
    analysis_request.channel_index = 2;
    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                   &analysis_result) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "analysis rejects an out-of-range channel") ||
        !check_condition(magnitudes[0] == 17.0f,
                         "invalid channel leaves output samples untouched")) {
        return EXIT_FAILURE;
    }
    analysis_request.channel_index = 0;
    analysis_request.magnitude_capacity = 14;
    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                   &analysis_result) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "analysis rejects insufficient output capacity") ||
        !check_condition(magnitudes[0] == 17.0f,
                         "insufficient capacity leaves output samples untouched")) {
        return EXIT_FAILURE;
    }
    analysis_request.magnitude_capacity = 21;
    analysis_request.output_magnitudes = tone_samples;
    if (!check_condition(audioforge_engine_analyze(&analysis_request,
                                                   &analysis_result) ==
                             AUDIOFORGE_STATUS_INVALID_ARGUMENT,
                         "analysis rejects output overlapping borrowed input") ||
        !check_condition(memcmp(tone_samples, original_tone_samples,
                                sizeof(tone_samples)) == 0,
                         "overlapping output cannot mutate input samples")) {
        return EXIT_FAILURE;
    }

    return 0;
}
