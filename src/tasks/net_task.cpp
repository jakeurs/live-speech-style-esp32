#include "tasks/net_task.h"
#include "tasks/ui_task.h"
#include "tasks/audio_task.h"
#include "net/http_client.h"
#include "net/response_parser.h"
#include "net/styles_api.h"
#include "app_state.h"
#include "util/log.h"
#include "util/request_id.h"
#include "audio/sfx.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Provided by main.cpp
extern AppCtx    g_app_ctx;
extern StyleList g_styles;

namespace {
int16_t* g_rb = nullptr;  size_t g_rb_cap = 0;
uint8_t* g_rpb = nullptr; size_t g_rpb_cap = 0;

struct SendCmd { size_t frames; };
QueueHandle_t g_q = nullptr;

void emit_sfx(SfxEvent e) {
    SfxCmd c{}; c.event = e;
    audio_task_enqueue(c);
}

void milestone(int m) {
    switch (m) {
        case 0:  // TCP connected
            emit_sfx(SfxEvent::TCP_CONNECTED);
            break;
        case 1:  // upload done
            emit_sfx(SfxEvent::UPLOAD_DONE);
            g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::UPLOAD_DONE);
            break;
        case 2:  // first byte
            emit_sfx(SfxEvent::SERVER_FIRST_BYTE);
            g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::SERVER_FIRST_BYTE);
            break;
        case 3:  // download done
            emit_sfx(SfxEvent::DOWNLOAD_DONE);
            g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::DOWNLOAD_DONE);
            break;
    }
}

void fail(const char* code, bool retryable) {
    g_app_ctx.last_error_retryable = retryable;
    AppEvent ev = retryable ? AppEvent::ERROR_RETRYABLE : AppEvent::ERROR_NON_RETRYABLE;
    g_app_ctx.state = app_on_event(g_app_ctx, ev);
    ui_task_set_error(code);
    emit_sfx(SfxEvent::ERROR);
}

void handle_send(const SendCmd& sc) {
    // Guard: reject obviously-too-short recordings (< 0.3 s)
    if (sc.frames < 4800) {
        log_line(LOG_WARN, "net", "reject_short", "frames=%u", (unsigned)sc.frames);
        fail("tooshort", false);
        return;
    }

    RestyleRequest req{};
    uint8_t idx = (g_app_ctx.style_idx < g_styles.count) ? g_app_ctx.style_idx : 0;
    req.style_id       = g_styles.count ? g_styles.ids[idx] : "jesus";
    req.language       = "en";
    req.pcm_bytes      = (const uint8_t*)g_rb;
    req.pcm_byte_count = sc.frames * sizeof(int16_t);
    request_id_new(req.request_id);

    log_line(LOG_INFO, "net", "submit",
             "style=%s frames=%u rid=%s",
             req.style_id, (unsigned)sc.frames, req.request_id);

    RestyleResult res{};
    res.response_buf = g_rpb;

    for (int attempt = 0; attempt < 2; attempt++) {
        g_app_ctx.state = AppState::UPLOADING;
        if (http_restyle(req, res, milestone)) {
            // 200 + audio/wav expected
            if (res.headers.status == 200
                && strncmp(res.headers.content_type, "audio/wav", 9) == 0) {
                if (res.response_len < 44 || !resp_is_wav(res.response_buf)) {
                    fail("badwav", false);
                    return;
                }
                SfxCmd c{};
                c.event           = SfxEvent::PLAYBACK_START_RESPONSE;
                c.response_pcm    = (const int16_t*)(res.response_buf + 44);
                c.response_frames = (uint32_t)((res.response_len - 44) / 2);
                audio_task_enqueue(c);
                log_line(LOG_INFO, "net", "ok",
                         "bytes=%u t_up=%u t_fb=%u t_dl=%u",
                         (unsigned)res.response_len, res.t_upload_ms,
                         res.t_first_byte_ms, res.t_download_ms);
                return;
            }
            // JSON body implies error
            JsonDocument d;
            if (!deserializeJson(d, res.response_buf, res.response_len)) {
                const char* code = d["error_code"] | "svrerror";
                bool retry = d["retryable"] | false;
                log_line(LOG_WARN, "net", "server_error",
                         "code=%s retry=%d attempt=%d", code, (int)retry, attempt);
                if (retry && attempt == 0) {
                    delay(1000);
                    continue;
                }
                fail(code, retry);
                return;
            }
            fail("badresp", false);
            return;
        }
        // Transport failure
        log_line(LOG_WARN, "net", "transport_fail", "attempt=%d", attempt);
        if (attempt == 0) {
            delay(1000);
            continue;
        }
        fail("netfail", false);
        return;
    }
}

void task_fn(void*) {
    SendCmd sc;
    while (true) {
        if (xQueueReceive(g_q, &sc, portMAX_DELAY) == pdTRUE) {
            handle_send(sc);
        }
    }
}
}  // namespace

void net_task_begin(int16_t* rb, size_t rb_bytes,
                    uint8_t* rpb, size_t rpb_bytes) {
    g_rb      = rb;
    g_rb_cap  = rb_bytes;
    g_rpb     = rpb;
    g_rpb_cap = rpb_bytes;
    g_q       = xQueueCreate(2, sizeof(SendCmd));
    xTaskCreatePinnedToCore(task_fn, "net", 8192, nullptr, 8, nullptr, 0);
}

void net_begin_send(size_t frames) {
    if (!g_q) return;
    SendCmd sc{frames};
    xQueueSend(g_q, &sc, 0);
}
