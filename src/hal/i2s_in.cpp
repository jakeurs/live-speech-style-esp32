#include "hal/i2s_in.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <cstring>
#include <cmath>

namespace {
constexpr i2s_port_t I2S_PORT = I2S_NUM_1;
constexpr size_t CHUNK_FRAMES = 512;

I2SInCtx* g_ctx = nullptr;
TaskHandle_t g_task = nullptr;
volatile bool g_run = false;

void task_fn(void*) {
    // INMP441-class mics output 24-bit samples left-justified in a 32-bit slot.
    // Reading as 32-bit then shifting down to int16 is the standard pattern.
    int32_t tmp[CHUNK_FRAMES];
    while (g_run) {
        if (!g_ctx) { delay(10); continue; }
        size_t bytes_read = 0;
        esp_err_t err = i2s_read(I2S_PORT, tmp, sizeof(tmp), &bytes_read, 100 / portTICK_PERIOD_MS);
        if (err != ESP_OK || bytes_read == 0) continue;
        size_t n = bytes_read / sizeof(int32_t);
        int16_t samples[CHUNK_FRAMES];
        for (size_t i = 0; i < n; i++) {
            // Shift 32-bit sample down to 16-bit (keep top 16 bits).
            samples[i] = (int16_t)(tmp[i] >> 16);
        }
        // RMS
        uint64_t acc = 0;
        for (size_t i = 0; i < n; i++) acc += (int32_t)samples[i] * samples[i];
        uint32_t rms = n ? (uint32_t)sqrtf((float)(acc / n)) : 0;
        if (rms > 65535) rms = 65535;
        g_ctx->last_rms.store((uint16_t)rms, std::memory_order_relaxed);

        // Append to buffer (truncate if full)
        size_t room = g_ctx->capacity_frames - g_ctx->write_pos_frames;
        size_t take = n < room ? n : room;
        if (take > 0) {
            memcpy(g_ctx->buf + g_ctx->write_pos_frames, samples, take * sizeof(int16_t));
            g_ctx->write_pos_frames += take;
        }
    }
    g_task = nullptr;
    vTaskDelete(nullptr);
}
}

bool i2s_in_begin(I2SInCtx* ctx) {
    g_ctx = ctx;
    g_ctx->write_pos_frames = 0;
    g_ctx->last_rms.store(0, std::memory_order_relaxed);

    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    cfg.sample_rate          = 16000;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;   // mic is 24-in-32
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = 0;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = CHUNK_FRAMES;
    cfg.use_apll             = false;
    cfg.fixed_mclk           = 0;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = 5;
    pins.ws_io_num    = 4;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num  = 6;
    pins.mck_io_num   = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    g_run = true;
    if (xTaskCreatePinnedToCore(task_fn, "i2s_in", 4096, nullptr, 10, &g_task, 1) != pdPASS) {
        g_run = false;
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    return true;
}

void i2s_in_stop() {
    g_run = false;
    for (int i = 0; i < 50 && g_task; i++) delay(10);
    i2s_driver_uninstall(I2S_PORT);
    g_ctx = nullptr;
}
