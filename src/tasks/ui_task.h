#pragma once
#include <cstdint>
#include <cstddef>
#include "app_state.h"
#include "net/styles_api.h"

using UiSendCallback = void (*)(size_t frames);

void ui_task_begin(StyleList* styles, AppCtx* ctx);
void ui_task_tick(uint32_t now_ms);
void ui_task_set_wifi(bool ok);
void ui_task_set_progress(uint32_t done, uint32_t total);
void ui_task_set_error(const char* code);
void ui_task_set_send_callback(UiSendCallback cb);   // Task 21 wires this to net_begin_send
void ui_task_start_health_spinner();
