#include "tasks/ui_task.h"
#include "hal/buttons.h"
#include "hal/oled.h"
#include "ui/render.h"
#include "tasks/audio_task.h"
#include "config/nvs_store.h"
#include "util/log.h"
#include <Arduino.h>
#include <cstring>

namespace {
StyleList*      g_styles = nullptr;
AppCtx*         g_ctx = nullptr;
UiModel         g_ui;
UiSendCallback  g_send_cb = nullptr;
uint32_t        g_last_render_ms = 0;
bool            g_wake_held = false;
uint32_t        g_record_started_ms = 0;

void apply_style_index() {
    if (!g_styles || g_styles->count == 0) {
        g_ui.style_name = "";
        return;
    }
    uint8_t i = g_ctx->style_idx;
    if (i >= g_styles->count) i = 0;
    g_ui.style_name = g_styles->ids[i];  // use id (short) on header
}

void send(size_t frames) {
    if (g_send_cb) g_send_cb(frames);
    else log_line(LOG_WARN, "ui", "send_cb_null", "frames=%u", (unsigned)frames);
}

void handle_button(const BtnEvent& e, uint32_t now_ms) {
    if (e.id == BtnId::WAKE) {
        if (e.ev == BtnEv::PRESS
            && g_ctx->state == AppState::IDLE
            && g_ctx->wifi_connected) {
            g_ctx->state = app_on_event(*g_ctx, AppEvent::WAKE_PRESS);
            g_wake_held = true;
            g_record_started_ms = now_ms;
            g_ui.record_started_ms = now_ms;
            audio_task_start_recording();
            SfxCmd c{}; c.event = SfxEvent::REC_START;
            audio_task_enqueue(c);
        } else if (e.ev == BtnEv::RELEASE && g_wake_held) {
            g_wake_held = false;
            if (g_ctx->state == AppState::RECORDING) {
                g_ctx->state = app_on_event(*g_ctx, AppEvent::WAKE_RELEASE);
                SfxCmd c{}; c.event = SfxEvent::SEND;
                audio_task_enqueue(c);
                size_t frames = audio_task_stop_recording();
                send(frames);
            }
        }
    } else if (e.ev == BtnEv::PRESS) {
        int dir = e.id == BtnId::VOL_UP ? +1 : -1;
        if (g_ctx->state == AppState::IDLE) {
            if (g_styles && g_styles->count) {
                int next = (int)g_ctx->style_idx + dir;
                if (next < 0) next = g_styles->count - 1;
                if (next >= g_styles->count) next = 0;
                g_ctx->style_idx = (uint8_t)next;
                apply_style_index();
                NvsState ns = nvs_load();
                ns.style_idx = g_ctx->style_idx;
                strncpy(ns.style_id, g_styles->ids[g_ctx->style_idx], sizeof(ns.style_id) - 1);
                ns.style_id[sizeof(ns.style_id) - 1] = 0;
                nvs_save(ns);
            }
        } else if (g_ctx->state == AppState::PLAYING) {
            int next = (int)g_ctx->volume_x10 + dir;
            if (next < 0) next = 0;
            if (next > 10) next = 10;
            g_ctx->volume_x10 = (uint8_t)next;
            audio_task_set_volume_x10(g_ctx->volume_x10);
            g_ui.vol_changed_ms = now_ms;
            NvsState ns = nvs_load();
            ns.volume_x10 = g_ctx->volume_x10;
            nvs_save(ns);
        }
    }
}
}

void ui_task_begin(StyleList* styles, AppCtx* ctx) {
    g_styles = styles;
    g_ctx = ctx;
    btns_begin();
    oled_begin();
    g_ui = UiModel{};
    apply_style_index();
}

void ui_task_set_wifi(bool ok) {
    g_ctx->wifi_connected = ok;
    g_ui.wifi_ok = ok;
    if (!ok && g_ctx->state != AppState::NO_WIFI) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::WIFI_LOST);
    } else if (ok && g_ctx->state == AppState::NO_WIFI) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::WIFI_OK);
    }
}
void ui_task_set_progress(uint32_t d, uint32_t t) { g_ui.bytes_done = d; g_ui.bytes_total = t; }
void ui_task_set_error(const char* c)             { g_ui.error_code = c ? c : ""; }
void ui_task_set_send_callback(UiSendCallback cb) { g_send_cb = cb; }

void ui_task_tick(uint32_t now_ms) {
    BtnEvent e;
    while (btns_poll(e)) handle_button(e, now_ms);

    // 60 s record cap
    if (g_ctx->state == AppState::RECORDING
        && now_ms - g_record_started_ms >= 60000) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::RECORD_CAP);
        SfxCmd c{}; c.event = SfxEvent::SEND;
        audio_task_enqueue(c);
        size_t frames = audio_task_stop_recording();
        send(frames);
    }

    // Refresh UI model from live sources
    g_ui.state = g_ctx->state;
    g_ui.volume_x10 = g_ctx->volume_x10;
    g_ui.now_ms = now_ms;
    g_ui.mic_rms = audio_task_mic_rms();
    g_ui.out_rms = audio_task_output_rms();

    // Render at ~20 Hz
    if (now_ms - g_last_render_ms >= 50) {
        ui_render(g_ui);
        g_last_render_ms = now_ms;
    }
}
