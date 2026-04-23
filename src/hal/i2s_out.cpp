#include "hal/i2s_out.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <cstring>

namespace {
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr size_t FRAMES = 512;

I2SOutFill g_fill = nullptr;
TaskHandle_t g_task = nullptr;
int16_t g_buf[FRAMES];
volatile bool g_run = false;

void task_fn(void*) {
    while (g_run) {
        if (g_fill) g_fill(g_buf, FRAMES);
        else        memset(g_buf, 0, sizeof(g_buf));
        size_t written = 0;
        i2s_write(I2S_PORT, g_buf, sizeof(g_buf), &written, portMAX_DELAY);
    }
    g_task = nullptr;
    vTaskDelete(nullptr);
}
}

bool i2s_out_begin(I2SOutFill fill) {
    g_fill = fill;

    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = 16000;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = 0;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = FRAMES;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 0;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = 15;
    pins.ws_io_num    = 16;
    pins.data_out_num = 7;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    pins.mck_io_num   = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    g_run = true;
    if (xTaskCreatePinnedToCore(task_fn, "i2s_out", 4096, nullptr, 10, &g_task, 1) != pdPASS) {
        g_run = false;
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }
    return true;
}

void i2s_out_end() {
    g_run = false;
    // Task deletes itself; wait briefly for it.
    for (int i = 0; i < 50 && g_task; i++) delay(10);
    i2s_driver_uninstall(I2S_PORT);
}
