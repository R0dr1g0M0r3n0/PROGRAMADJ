#include "audioforge/engine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

extern "C" AudioForgeStatus audioforge_gain_apply(
    const AudioForgeAudioBuffer* input,
    float gain,
    AudioForgeAudioBufferOutput* output);

static bool check(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
    return condition;
}

static AudioForgeAudioBufferOutput output_for(float* samples,
                                               uint64_t frame_capacity,
                                               uint32_t channel_count = 1,
                                               uint32_t sample_rate = 48000) {
    AudioForgeAudioBufferOutput output = AUDIOFORGE_AUDIO_BUFFER_OUTPUT_INIT;
    output.samples = samples;
    output.frame_capacity = frame_capacity;
    output.channel_count = channel_count;
    output.sample_rate = sample_rate;
    return output;
}

int main() {
    const float impulse_samples[] = {1.0f, 0.0f, 0.0f};
    AudioForgeAudioBuffer impulse = AUDIOFORGE_AUDIO_BUFFER_INIT;
    if (!check(audioforge_audio_buffer_import(impulse_samples, 3, 1, 48000,
                                              &impulse) == AUDIOFORGE_STATUS_OK,
               "impulse fixture imports")) {
        return EXIT_FAILURE;
    }

    float impulse_output[3] = {0.0f, 0.0f, 0.0f};
    auto impulse_destination = output_for(impulse_output, 3);
    if (!check(audioforge_gain_apply(&impulse, 0.5f, &impulse_destination) ==
                   AUDIOFORGE_STATUS_OK,
               "gain accepts impulse input") ||
        !check(impulse_destination.frame_count == 3,
               "gain preserves impulse frame count") ||
        !check(impulse_output[0] == 0.5f && impulse_output[1] == 0.0f &&
                   impulse_output[2] == 0.0f,
               "gain applies deterministic impulse response")) {
        return EXIT_FAILURE;
    }

    const float silence_samples[] = {0.0f, 0.0f, 0.0f};
    AudioForgeAudioBuffer silence = AUDIOFORGE_AUDIO_BUFFER_INIT;
    if (!check(audioforge_audio_buffer_import(silence_samples, 3, 1, 48000,
                                              &silence) == AUDIOFORGE_STATUS_OK,
               "silence fixture imports")) {
        return EXIT_FAILURE;
    }

    float silence_output[3] = {9.0f, 9.0f, 9.0f};
    auto silence_destination = output_for(silence_output, 3);
    if (!check(audioforge_gain_apply(&silence, 4.0f, &silence_destination) ==
                   AUDIOFORGE_STATUS_OK,
               "gain accepts silence input") ||
        !check(silence_output[0] == 0.0f && silence_output[1] == 0.0f &&
                   silence_output[2] == 0.0f,
               "gain preserves silence")) {
        return EXIT_FAILURE;
    }

    float guarded_output[3] = {7.0f, 7.0f, 7.0f};
    auto insufficient_destination = output_for(guarded_output, 2);
    if (!check(audioforge_gain_apply(&impulse, 1.0f, &insufficient_destination) ==
                   AUDIOFORGE_STATUS_INVALID_ARGUMENT,
               "gain rejects insufficient output capacity") ||
        !check(guarded_output[0] == 7.0f && guarded_output[1] == 7.0f,
               "capacity failure leaves output samples untouched")) {
        return EXIT_FAILURE;
    }

    auto invalid_gain_destination = output_for(guarded_output, 3);
    if (!check(audioforge_gain_apply(&impulse, NAN, &invalid_gain_destination) ==
                   AUDIOFORGE_STATUS_INVALID_ARGUMENT,
               "gain rejects non-finite gain")) {
        return EXIT_FAILURE;
    }

    auto mismatched_destination = output_for(guarded_output, 3, 2);
    if (!check(audioforge_gain_apply(&impulse, 1.0f, &mismatched_destination) ==
                   AUDIOFORGE_STATUS_INVALID_ARGUMENT,
               "gain rejects format mismatch") ||
        !check(mismatched_destination.frame_count == 0,
               "format failure clears output frame count")) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
