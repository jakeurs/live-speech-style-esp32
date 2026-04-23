#pragma once
#include <cstdint>
#include "app_state.h"

struct UiModel {
    AppState    state = AppState::IDLE;
    const char* style_name = "";
    bool        wifi_ok = false;
    uint16_t    mic_rms = 0;
    uint16_t    out_rms = 0;
    uint32_t    bytes_done = 0;
    uint32_t    bytes_total = 0;
    uint8_t     volume_x10 = 6;
    uint32_t    now_ms = 0;
    uint32_t    vol_changed_ms = 0;
    uint32_t    record_started_ms = 0;
    const char* error_code = "";
};

void ui_render(const UiModel& m);
