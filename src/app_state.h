#pragma once
#include <cstdint>

enum class AppState : uint8_t {
    IDLE, RECORDING, UPLOADING, WAITING, DOWNLOADING, PLAYING,
    RETRY, ERROR, NO_WIFI,
};

enum class AppEvent : uint8_t {
    WAKE_PRESS, WAKE_RELEASE, RECORD_CAP,
    UPLOAD_DONE, SERVER_FIRST_BYTE, DOWNLOAD_DONE, PLAYBACK_END,
    ERROR_RETRYABLE, ERROR_NON_RETRYABLE,
    RETRY_TICK,
    WIFI_LOST, WIFI_OK,
    VOL_UP, VOL_DOWN,
    ERROR_TIMEOUT,
};

struct AppCtx {
    AppState state = AppState::IDLE;
    bool     wifi_connected = false;
    uint8_t  retries_used = 0;
    bool     last_error_retryable = false;
    uint8_t  style_idx = 0;
    uint8_t  volume_x10 = 6;
};

AppState app_on_event(AppCtx& ctx, AppEvent ev);
const char* app_state_name(AppState s);
