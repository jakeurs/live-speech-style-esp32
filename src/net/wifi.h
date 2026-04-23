#pragma once
#include <cstdint>

using WifiCallback = void (*)(bool connected);

void wifi_begin(const char* ssid, const char* pass, WifiCallback cb);
bool wifi_connected();
