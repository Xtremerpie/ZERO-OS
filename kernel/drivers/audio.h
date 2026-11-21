/* ZERO KERNEL - Audio Device Subsystem Abstraction (Milestone 9.0.0 ZERO Ultimate).
 *
 * Implements hardware audio discovery (Intel HDA / AC'97), standard PCM buffer
 * management, volume/mute controls, underrun protection, and software fallback null sink.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define AUDIO_BUFFER_SIZE_BYTES 8192u
#define AUDIO_SAMPLE_RATE_44K   44100u
#define AUDIO_SAMPLE_RATE_48K   48000u

typedef enum {
    AUDIO_DEVICE_NONE = 0,
    AUDIO_DEVICE_INTEL_HDA,    /* Intel High Definition Audio */
    AUDIO_DEVICE_AC97,         /* AC'97 Audio Controller */
    AUDIO_DEVICE_NULL_SINK     /* Safe software fallback sink */
} audio_device_type_t;

typedef struct audio_device {
    char                name[48];
    audio_device_type_t type;
    uint16_t            vendor_id;
    uint16_t            device_id;
    uint32_t            sample_rate;
    uint8_t             channels;         /* 1 = Mono, 2 = Stereo */
    uint8_t             bits_per_sample;  /* 16-bit PCM standard */
    uint8_t             volume_percent;   /* 0 - 100 */
    bool                is_muted;
    bool                is_playing;
    uint64_t            bytes_played;
    uint32_t            underruns_count;
    bool                initialized;
} audio_device_t;

typedef struct audio_telemetry {
    char     active_device_name[48];
    uint32_t sample_rate;
    uint8_t  volume_percent;
    bool     is_muted;
    uint64_t total_samples_rendered;
    uint32_t underruns;
    bool     hardware_present;
} audio_telemetry_t;

/* Initialize audio subsystem and probe audio hardware */
void audio_init(void);

/* Query primary audio device descriptor */
const audio_device_t *audio_get_device(void);

/* Query audio telemetry */
void audio_get_telemetry(audio_telemetry_t *out_telem);

/* Adjust master audio playback volume (0 - 100%) */
bool audio_set_volume(uint8_t volume_percent);
uint8_t audio_get_volume(void);

/* Toggle audio mute state */
void audio_set_mute(bool muted);
bool audio_is_muted(void);

/* Submit PCM 16-bit stereo audio data to audio output buffer */
int32_t audio_write_pcm(const int16_t *pcm_samples, size_t sample_count);
