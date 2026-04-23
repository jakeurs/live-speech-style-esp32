#include "net/wifi.h"
#include <WiFi.h>
#include "util/log.h"

namespace {
WifiCallback g_cb = nullptr;

void evt(WiFiEvent_t e, WiFiEventInfo_t) {
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        log_line(LOG_INFO, "wifi", "got_ip", "ip=%s", WiFi.localIP().toString().c_str());
        if (g_cb) g_cb(true);
    } else if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        log_line(LOG_WARN, "wifi", "disconnected", "");
        if (g_cb) g_cb(false);
    }
}
}

void wifi_begin(const char* ssid, const char* pass, WifiCallback cb) {
    g_cb = cb;
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(evt);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, pass);
    log_line(LOG_INFO, "wifi", "connecting", "ssid=%s", ssid);
}

bool wifi_connected() { return WiFi.status() == WL_CONNECTED; }
