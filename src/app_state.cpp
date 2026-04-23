#include "app_state.h"

static bool is_busy(AppState s) {
    return s == AppState::RECORDING || s == AppState::UPLOADING
        || s == AppState::WAITING   || s == AppState::DOWNLOADING;
}

AppState app_on_event(AppCtx& c, AppEvent ev) {
    if (ev == AppEvent::WIFI_LOST && is_busy(c.state)) return AppState::ERROR;
    if (ev == AppEvent::WIFI_LOST && c.state != AppState::NO_WIFI) return AppState::NO_WIFI;
    if (ev == AppEvent::WIFI_OK && c.state == AppState::NO_WIFI) return AppState::IDLE;

    switch (c.state) {
        case AppState::IDLE:
            if (ev == AppEvent::WAKE_PRESS && c.wifi_connected) return AppState::RECORDING;
            return c.state;
        case AppState::RECORDING:
            if (ev == AppEvent::WAKE_RELEASE || ev == AppEvent::RECORD_CAP)
                return AppState::UPLOADING;
            return c.state;
        case AppState::UPLOADING:
            if (ev == AppEvent::UPLOAD_DONE) return AppState::WAITING;
            if (ev == AppEvent::ERROR_RETRYABLE) {
                if (c.retries_used == 0 && c.last_error_retryable) return AppState::RETRY;
                return AppState::ERROR;
            }
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::WAITING:
            if (ev == AppEvent::SERVER_FIRST_BYTE) return AppState::DOWNLOADING;
            if (ev == AppEvent::ERROR_RETRYABLE) {
                if (c.retries_used == 0 && c.last_error_retryable) return AppState::RETRY;
                return AppState::ERROR;
            }
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::DOWNLOADING:
            if (ev == AppEvent::DOWNLOAD_DONE) return AppState::PLAYING;
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::PLAYING:
            if (ev == AppEvent::PLAYBACK_END) return AppState::IDLE;
            return c.state;
        case AppState::RETRY:
            if (ev == AppEvent::RETRY_TICK) {
                c.retries_used++;
                return AppState::UPLOADING;
            }
            return c.state;
        case AppState::ERROR:
            if (ev == AppEvent::RETRY_TICK) {
                c.retries_used = 0;
                c.last_error_retryable = false;
                return AppState::IDLE;
            }
            return c.state;
        case AppState::NO_WIFI:
            return c.state;
    }
    return c.state;
}

const char* app_state_name(AppState s) {
    switch (s) {
        case AppState::IDLE:        return "IDLE";
        case AppState::RECORDING:   return "RECORDING";
        case AppState::UPLOADING:   return "UPLOADING";
        case AppState::WAITING:     return "WAITING";
        case AppState::DOWNLOADING: return "DOWNLOADING";
        case AppState::PLAYING:     return "PLAYING";
        case AppState::RETRY:       return "RETRY";
        case AppState::ERROR:       return "ERROR";
        case AppState::NO_WIFI:     return "NO_WIFI";
    }
    return "?";
}
