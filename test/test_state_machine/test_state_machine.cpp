#include <unity.h>
#include <initializer_list>
#include "app_state.h"

void setUp() {}
void tearDown() {}

void test_idle_wake_press_starts_recording() {
    AppCtx c{};
    c.state = AppState::IDLE;
    c.wifi_connected = true;
    TEST_ASSERT_EQUAL(AppState::RECORDING, app_on_event(c, AppEvent::WAKE_PRESS));
}

void test_idle_wake_ignored_without_wifi() {
    AppCtx c{};
    c.state = AppState::IDLE;
    c.wifi_connected = false;
    TEST_ASSERT_EQUAL(AppState::IDLE, app_on_event(c, AppEvent::WAKE_PRESS));
}

void test_recording_release_to_uploading() {
    AppCtx c{};
    c.state = AppState::RECORDING;
    TEST_ASSERT_EQUAL(AppState::UPLOADING, app_on_event(c, AppEvent::WAKE_RELEASE));
}

void test_recording_cap_to_uploading() {
    AppCtx c{};
    c.state = AppState::RECORDING;
    TEST_ASSERT_EQUAL(AppState::UPLOADING, app_on_event(c, AppEvent::RECORD_CAP));
}

void test_uploading_to_waiting() {
    AppCtx c{}; c.state = AppState::UPLOADING;
    TEST_ASSERT_EQUAL(AppState::WAITING, app_on_event(c, AppEvent::UPLOAD_DONE));
}

void test_waiting_first_byte_to_downloading() {
    AppCtx c{}; c.state = AppState::WAITING;
    TEST_ASSERT_EQUAL(AppState::DOWNLOADING, app_on_event(c, AppEvent::SERVER_FIRST_BYTE));
}

void test_downloading_to_playing() {
    AppCtx c{}; c.state = AppState::DOWNLOADING;
    TEST_ASSERT_EQUAL(AppState::PLAYING, app_on_event(c, AppEvent::DOWNLOAD_DONE));
}

void test_playing_end_to_idle() {
    AppCtx c{}; c.state = AppState::PLAYING;
    TEST_ASSERT_EQUAL(AppState::IDLE, app_on_event(c, AppEvent::PLAYBACK_END));
}

void test_retryable_error_goes_to_error() {
    AppCtx c{};
    c.state = AppState::WAITING;
    c.last_error_retryable = true;
    TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::ERROR_RETRYABLE));
}

void test_non_retryable_error_goes_straight_to_error() {
    AppCtx c{};
    c.state = AppState::WAITING;
    TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::ERROR_NON_RETRYABLE));
}

void test_error_clear_returns_to_idle() {
    AppCtx c{};
    c.state = AppState::ERROR;
    c.last_error_retryable = true;
    TEST_ASSERT_EQUAL(AppState::IDLE, app_on_event(c, AppEvent::ERROR_CLEAR));
    TEST_ASSERT_FALSE(c.last_error_retryable);
}

void test_wifi_loss_during_any_busy_state_goes_error() {
    AppCtx c{};
    for (auto s : {AppState::RECORDING, AppState::UPLOADING,
                   AppState::WAITING, AppState::DOWNLOADING}) {
        c.state = s;
        TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::WIFI_LOST));
    }
}

void test_vol_buttons_ignored_outside_idle_playing() {
    AppCtx c{}; c.state = AppState::WAITING;
    TEST_ASSERT_EQUAL(AppState::WAITING, app_on_event(c, AppEvent::VOL_UP));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_wake_press_starts_recording);
    RUN_TEST(test_idle_wake_ignored_without_wifi);
    RUN_TEST(test_recording_release_to_uploading);
    RUN_TEST(test_recording_cap_to_uploading);
    RUN_TEST(test_uploading_to_waiting);
    RUN_TEST(test_waiting_first_byte_to_downloading);
    RUN_TEST(test_downloading_to_playing);
    RUN_TEST(test_playing_end_to_idle);
    RUN_TEST(test_retryable_error_goes_to_error);
    RUN_TEST(test_non_retryable_error_goes_straight_to_error);
    RUN_TEST(test_error_clear_returns_to_idle);
    RUN_TEST(test_wifi_loss_during_any_busy_state_goes_error);
    RUN_TEST(test_vol_buttons_ignored_outside_idle_playing);
    return UNITY_END();
}
