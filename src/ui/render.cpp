#include "ui/render.h"
#include "hal/oled.h"
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* SPINNER_FRAMES[] = {"-", "\\", "|", "/", "-", "\\", "|", "/", ""};
constexpr int         SPINNER_COUNT    = 9;
constexpr uint32_t    SPINNER_FRAME_MS = 150;

void maybe_spinner(const UiModel& m) {
    if (m.state != AppState::IDLE)                return;
    if (m.health_spinner_started_ms == 0)         return;
    if (m.now_ms - m.vol_changed_ms < 1000)       return;  // vol overlay has priority
    uint32_t elapsed = m.now_ms - m.health_spinner_started_ms;
    int frame = (int)(elapsed / SPINNER_FRAME_MS);
    if (frame < 0 || frame >= SPINNER_COUNT)      return;
    const char* ch = SPINNER_FRAMES[frame];
    if (!ch[0])                                   return;
    oled_text(116, 48, 2, ch);
}

// Render size-2 text from the column that centers it (best-effort).
// n chars * 12 px = n*12 total width; start x = (128 - n*12) / 2.
void center_size2(int y, const char* s) {
    int n = (int)strlen(s);
    if (n > 10) n = 10;
    int x = (128 - n * 12) / 2;
    if (x < 0) x = 0;
    char trunc[11];
    strncpy(trunc, s, 10);
    trunc[n] = 0;
    oled_text(x, y, 2, trunc);
}

// Map an RMS (0..65535) to a 0..255 fill fraction on a log-ish scale.
uint8_t rms_to_fill(uint16_t rms) {
    if (rms < 50) return 0;
    // 20-step log-ish scale, then map to 0..255
    uint32_t r = rms;
    uint8_t n = 0;
    while (r > 50 && n < 20) { r = (r * 7) / 10; n++; }
    return (uint8_t)((uint32_t)n * 255 / 20);
}

void draw_progress(uint32_t done, uint32_t total) {
    if (total == 0) return;
    uint8_t fill = (uint8_t)((uint64_t)done * 255 / total);
    oled_hbar(4, 36, 120, 8, fill);
}

void draw_vu(uint16_t rms) {
    oled_hbar(4, 36, 120, 8, rms_to_fill(rms));
}

void draw_sweeper(uint32_t now_ms) {
    int pos = (int)((now_ms / 20) % 112);   // 0..111
    oled_rect(4 + pos, 36, 8, 8, true);
}

void draw_row1_style(const char* name) {
    if (!name || !name[0]) return;
    // Left-aligned in yellow band (no centering — feels more app-like here)
    char trunc[11];
    strncpy(trunc, name, 10);
    trunc[10] = 0;
    oled_text(0, 0, 2, trunc);
}

void maybe_vol_overlay(const UiModel& m) {
    if (m.now_ms - m.vol_changed_ms >= 1000) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "vol %u/10", (unsigned)m.volume_x10);
    center_size2(48, buf);
}

}  // namespace

void ui_render(const UiModel& m) {
    oled_clear();

    // Row 1 — style name (except NO_WIFI)
    if (m.state != AppState::NO_WIFI) draw_row1_style(m.style_name);

    // Row 2 — state word (centered)
    switch (m.state) {
        case AppState::IDLE:        center_size2(16, "READY"); break;
        case AppState::UPLOADING:   center_size2(16, "SENDING"); break;
        case AppState::WAITING:     center_size2(16, "THINKING"); break;
        case AppState::DOWNLOADING: center_size2(16, "RECVING"); break;
        case AppState::PLAYING:     center_size2(16, "PLAYING"); break;
        case AppState::ERROR:       center_size2(16, "ERROR"); break;
        case AppState::NO_WIFI:     center_size2(16, "NO WIFI"); break;
        case AppState::RECORDING: {
            char buf[16];
            uint32_t secs = (m.now_ms - m.record_started_ms) / 1000;
            snprintf(buf, sizeof(buf), "REC %02u:%02u",
                     (unsigned)(secs / 60), (unsigned)(secs % 60));
            center_size2(16, buf);
            break;
        }
    }

    // Row 3 — bar/animation
    switch (m.state) {
        case AppState::RECORDING:   draw_vu(m.mic_rms); break;
        case AppState::PLAYING:     draw_vu(m.out_rms); break;
        case AppState::UPLOADING:
        case AppState::DOWNLOADING: draw_progress(m.bytes_done, m.bytes_total); break;
        case AppState::WAITING:     draw_sweeper(m.now_ms); break;
        default: break;
    }

    // Row 4 — error/vol/blank
    if (m.state == AppState::ERROR) {
        center_size2(48, m.error_code ? m.error_code : "");
    } else if (m.state == AppState::IDLE || m.state == AppState::PLAYING) {
        maybe_vol_overlay(m);
    }

    maybe_spinner(m);
    oled_show();
}
