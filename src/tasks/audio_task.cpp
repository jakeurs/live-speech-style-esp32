#include "tasks/audio_task.h"
#include "hal/i2s_out.h"
#include "audio/mixer.h"
#include "util/log.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cmath>

namespace {
QueueHandle_t g_sfx_q = nullptr;
I2SInCtx      g_rec_ctx{};
std::atomic<uint16_t> g_out_rms{0};
std::atomic<uint8_t>  g_vol_x10{6};
bool g_recording = false;
std::atomic<bool> g_response_playing{false};
std::atomic<bool> g_response_just_finished{false};

void on_fill(int16_t* buf, size_t frames) {
    SfxCmd c;
    while (xQueueReceive(g_sfx_q, &c, 0) == pdTRUE) {
        if (c.event == SfxEvent::PLAYBACK_START_RESPONSE) g_response_playing.store(true);
        sfx_apply(c);
    }
    bool was_active = mixer_voice_active(0);
    float v = g_vol_x10.load() / 10.0f;
    mixer_render(buf, frames, v);
    bool still_active = mixer_voice_active(0);
    if (g_response_playing.load() && was_active && !still_active) {
        g_response_playing.store(false);
        g_response_just_finished.store(true);
    }
    uint64_t acc = 0;
    for (size_t i = 0; i < frames; i++) acc += (int32_t)buf[i] * buf[i];
    uint32_t rms = frames ? (uint32_t)sqrtf((float)(acc / frames)) : 0;
    if (rms > 65535) rms = 65535;
    g_out_rms.store((uint16_t)rms, std::memory_order_relaxed);
}
}

void audio_task_begin(int16_t* rb, size_t cap) {
    g_rec_ctx.buf = rb;
    g_rec_ctx.capacity_frames = cap;
    g_rec_ctx.write_pos_frames = 0;
    g_sfx_q = xQueueCreate(16, sizeof(SfxCmd));
    sfx_init();
    if (!i2s_out_begin(on_fill)) {
        log_line(LOG_ERROR, "audio", "i2s_out_begin failed", "");
    }
}

void audio_task_enqueue(const SfxCmd& c) {
    if (g_sfx_q) xQueueSend(g_sfx_q, &c, 0);
}

void audio_task_start_recording() {
    if (g_recording) return;
    g_rec_ctx.write_pos_frames = 0;
    if (i2s_in_begin(&g_rec_ctx)) g_recording = true;
}

size_t audio_task_stop_recording() {
    if (!g_recording) return 0;
    size_t n = g_rec_ctx.write_pos_frames;
    i2s_in_stop();
    g_recording = false;
    return n;
}

uint16_t audio_task_mic_rms()    { return g_rec_ctx.last_rms.load(std::memory_order_relaxed); }
uint16_t audio_task_output_rms() { return g_out_rms.load(std::memory_order_relaxed); }
void audio_task_set_volume_x10(uint8_t v) { g_vol_x10.store(v > 10 ? 10 : v); }

bool audio_task_consume_playback_end() {
    bool f = g_response_just_finished.load();
    if (f) g_response_just_finished.store(false);
    return f;
}
