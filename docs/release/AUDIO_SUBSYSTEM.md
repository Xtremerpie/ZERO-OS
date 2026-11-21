# ZERO OS — Audio Device Subsystem Specification
**Milestone:** 9.0.0 — ZERO Ultimate  
**Component:** `kernel/drivers/audio.h`, `audio.c`  

---

## 1. Overview

Milestone 9.0.0 establishes the native Audio Subsystem abstraction for ZERO OS. Designed to prevent gaming audio stalls and system crashes on hardware lacking native audio drivers, the audio subsystem implements hardware discovery (Intel High Definition Audio and AC'97) paired with a safe, non-blocking software null sink fallback.

---

## 2. Audio Device Interface

```c
typedef struct audio_device {
    char                name[48];
    audio_device_type_t type;             /* INTEL_HDA, AC97, NULL_SINK */
    uint16_t            vendor_id;
    uint16_t            device_id;
    uint32_t            sample_rate;      /* 44100 Hz or 48000 Hz */
    uint8_t             channels;         /* 2 (Stereo) */
    uint8_t             bits_per_sample;  /* 16-bit PCM standard */
    uint8_t             volume_percent;   /* 0 - 100 */
    bool                is_muted;
    bool                is_playing;
    uint64_t            bytes_played;
    uint32_t            underruns_count;
    bool                initialized;
} audio_device_t;
```

---

## 3. Operational Guarantees

1. **Non-Blocking PCM Streaming:** The audio ring buffer (`audio_write_pcm()`) accepts 16-bit signed stereo PCM samples and writes them to an 8,192-byte ring buffer without yielding the calling thread or causing scheduler latency jitter.
2. **Safe Fallback Sink:** When physical audio hardware is absent or uninitialized (e.g., standard QEMU default configuration), `AUDIO_DEVICE_NULL_SINK` absorbs audio samples, updates sample telemetry, and returns success, guaranteeing that game loops and media applications run without audio-related aborts.
3. **Master Volume & Muting:** Volume controls clamp between 0% and 100%. Muting silently drops audio buffer updates while preserving stream timing.
