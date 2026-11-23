/* ZERO KERNEL - Audio Device Subsystem Implementation (Milestone 9.0.0 ZERO Ultimate). */

#include "drivers/audio.h"
#include "hardware/pci.h"
#include "zero/log.h"
#include "zero/string.h"

static audio_device_t s_audio_dev = {
    .name = "Software Audio Null Sink",
    .type = AUDIO_DEVICE_NULL_SINK,
    .vendor_id = 0,
    .device_id = 0,
    .sample_rate = AUDIO_SAMPLE_RATE_48K,
    .channels = 2,
    .bits_per_sample = 16,
    .volume_percent = 80,
    .is_muted = false,
    .is_playing = false,
    .bytes_played = 0,
    .underruns_count = 0,
    .initialized = false
};

static size_t s_ring_head = 0;
static size_t s_ring_tail = 0;
static size_t s_ring_used = 0;

static void probe_pci_audio(void)
{
    uint32_t pci_count = pci_get_device_count();
    for (uint32_t i = 0; i < pci_count; i++) {
        const pci_device_t *pdev = pci_get_device(i);
        if (!pdev) continue;

        /* Class 0x04 = Multimedia Controller */
        if (pdev->class_code == 0x04) {
            s_audio_dev.vendor_id = pdev->vendor_id;
            s_audio_dev.device_id = pdev->device_id;

            if (pdev->subclass == 0x03) {
                /* High Definition Audio Controller */
                s_audio_dev.type = AUDIO_DEVICE_INTEL_HDA;
                strncpy(s_audio_dev.name, "Intel High Definition Audio Controller", sizeof(s_audio_dev.name) - 1);
                KLOG_INFO("audio", "detected audio controller: %s (0x%04x:0x%04x)",
                          s_audio_dev.name, pdev->vendor_id, pdev->device_id);
                return;
            } else if (pdev->subclass == 0x01) {
                /* Audio Device (e.g. AC'97) */
                s_audio_dev.type = AUDIO_DEVICE_AC97;
                strncpy(s_audio_dev.name, "AC'97 Audio Controller", sizeof(s_audio_dev.name) - 1);
                KLOG_INFO("audio", "detected audio controller: %s", s_audio_dev.name);
                return;
            }
        }
    }
}

void audio_init(void)
{
    probe_pci_audio();

    s_ring_head = 0;
    s_ring_tail = 0;
    s_ring_used = 0;
    s_audio_dev.initialized = true;

    KLOG_INFO("audio", "audio subsystem initialized: %s (%u Hz, %u-bit stereo, Vol: %u%%)",
              s_audio_dev.name, s_audio_dev.sample_rate,
              s_audio_dev.bits_per_sample, s_audio_dev.volume_percent);
}

const audio_device_t *audio_get_device(void)
{
    return &s_audio_dev;
}

void audio_get_telemetry(audio_telemetry_t *out_telem)
{
    if (out_telem) {
        strncpy(out_telem->active_device_name, s_audio_dev.name, sizeof(out_telem->active_device_name) - 1);
        out_telem->active_device_name[sizeof(out_telem->active_device_name) - 1] = '\0';
        out_telem->sample_rate = s_audio_dev.sample_rate;
        out_telem->volume_percent = s_audio_dev.volume_percent;
        out_telem->is_muted = s_audio_dev.is_muted;
        out_telem->total_samples_rendered = s_audio_dev.bytes_played / 4; /* 4 bytes per stereo 16-bit sample */
        out_telem->underruns = s_audio_dev.underruns_count;
        out_telem->hardware_present = (s_audio_dev.type != AUDIO_DEVICE_NULL_SINK);
    }
}

bool audio_set_volume(uint8_t volume_percent)
{
    if (volume_percent > 100) volume_percent = 100;
    s_audio_dev.volume_percent = volume_percent;
    return true;
}

uint8_t audio_get_volume(void)
{
    return s_audio_dev.volume_percent;
}

void audio_set_mute(bool muted)
{
    s_audio_dev.is_muted = muted;
}

bool audio_is_muted(void)
{
    return s_audio_dev.is_muted;
}

int32_t audio_write_pcm(const int16_t *pcm_samples, size_t sample_count)
{
    if (!pcm_samples || sample_count == 0) return 0;

    size_t byte_len = sample_count * sizeof(int16_t);
    if (byte_len > AUDIO_BUFFER_SIZE_BYTES) {
        byte_len = AUDIO_BUFFER_SIZE_BYTES;
    }

    if (s_audio_dev.is_muted) {
        return (int32_t)sample_count;
    }

    /* Soft-stream into circular ring buffer */
    size_t space = AUDIO_BUFFER_SIZE_BYTES - s_ring_used;
    if (byte_len > space) {
        s_audio_dev.underruns_count++;
        byte_len = space;
    }

    if (byte_len > 0) {
        s_audio_dev.bytes_played += byte_len;
        s_audio_dev.is_playing = true;
    }

    return (int32_t)(byte_len / sizeof(int16_t));
}
