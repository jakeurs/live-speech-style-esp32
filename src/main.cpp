#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include "tasks/ui_task.h"
#include "tasks/audio_task.h"
#include "tasks/net_task.h"
#include "net/wifi.h"
#include "net/styles_api.h"
#include "config/nvs_store.h"
#include "util/log.h"
#include "util/request_id.h"
#include "secrets.h"

// Exported globals for net_task.cpp's extern references
AppCtx    g_app_ctx;
StyleList g_styles{};

static int16_t* g_record_buf   = nullptr;
static uint8_t* g_response_buf = nullptr;

static constexpr size_t RECORD_BYTES   = 1920000;  // 60 s @ 16 kHz mono int16
static constexpr size_t RESPONSE_BYTES = 2000000;  // 2 MB cap

static void wifi_cb(bool ok) { ui_task_set_wifi(ok); }

void setup() {
    Serial.begin(115200); delay(500);
    log_init_arduino();
    request_id_seed(esp_random());
    nvs_set_clock_ms([]() -> uint32_t { return millis(); });

    g_record_buf   = (int16_t*)heap_caps_malloc(RECORD_BYTES,   MALLOC_CAP_SPIRAM);
    g_response_buf = (uint8_t*)heap_caps_malloc(RESPONSE_BYTES, MALLOC_CAP_SPIRAM);
    if (!g_record_buf || !g_response_buf) {
        log_line(LOG_ERROR, "main", "psram_alloc_fail", "");
        while (true) delay(1000);
    }

    NvsState ns = nvs_load();
    g_app_ctx.style_idx  = (uint8_t)ns.style_idx;
    g_app_ctx.volume_x10 = ns.volume_x10;

    audio_task_begin(g_record_buf, RECORD_BYTES / sizeof(int16_t));
    audio_task_set_volume_x10(g_app_ctx.volume_x10);

    ui_task_begin(&g_styles, &g_app_ctx);
    ui_task_set_send_callback(net_begin_send);

    wifi_begin(WIFI_SSID, WIFI_PASS, wifi_cb);

    net_task_begin(g_record_buf, RECORD_BYTES,
                   g_response_buf, RESPONSE_BYTES);

    log_line(LOG_INFO, "main", "boot_done",
             "rec=%u resp=%u", (unsigned)RECORD_BYTES, (unsigned)RESPONSE_BYTES);
}

void loop() {
    ui_task_tick(millis());

    // Fetch the styles list once WiFi is up. Retry every 30 s until successful,
    // so boot-time server flakiness doesn't leave the device permanently style-less.
    // Done here, NOT in the wifi callback — that runs on arduino_events task
    // which has a tiny stack.
    static uint32_t g_last_style_attempt_ms = 0;
    if (g_styles.count == 0 && wifi_connected()) {
        uint32_t now = millis();
        if (g_last_style_attempt_ms == 0 || now - g_last_style_attempt_ms >= 30000) {
            g_last_style_attempt_ms = now;
            if (styles_fetch(g_styles)) {
                log_line(LOG_INFO, "main", "styles_loaded",
                         "count=%u", (unsigned)g_styles.count);
                if (g_app_ctx.style_idx >= g_styles.count) {
                    g_app_ctx.style_idx = 0;
                    NvsState ns = nvs_load();
                    ns.style_idx = 0;
                    strncpy(ns.style_id, g_styles.ids[0], sizeof(ns.style_id) - 1);
                    ns.style_id[sizeof(ns.style_id) - 1] = 0;
                    nvs_save(ns);
                }
            } else {
                log_line(LOG_WARN, "main", "styles_load_fail", "");
            }
        }
    }

    static uint32_t g_last_health_ms = 0;
    if (g_app_ctx.state == AppState::IDLE
        && wifi_connected()
        && g_styles.count > 0) {
        uint32_t now = millis();
        if (g_last_health_ms == 0 || now - g_last_health_ms >= 60000) {
            g_last_health_ms = now;
            HealthStatus h{};
            if (health_fetch(h)) {
                log_line(LOG_INFO, "main", "health_ok",
                         "stt=%d llm=%d tts=%d", h.stt_ok, h.llm_ok, h.tts_ok);
                ui_task_start_health_spinner();
            } else {
                log_line(LOG_WARN, "main", "health_fail", "");
            }
        }
    }

    delay(5);
}
