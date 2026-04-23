#include "net/styles_api.h"
#include <ArduinoJson.h>
#include <cstring>

bool styles_parse_list(const char* body, size_t len, StyleList& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len)) return false;
    if (!doc.is<JsonArray>()) return false;
    JsonArray arr = doc.as<JsonArray>();
    out.count = 0;
    for (JsonObject o : arr) {
        if (out.count >= 16) break;
        const char* id = o["id"] | "";
        const char* nm = o["name"] | "";
        if (!id[0]) continue;
        strncpy(out.ids[out.count], id, 32);
        out.ids[out.count][32] = 0;
        strncpy(out.names[out.count], nm, 32);
        out.names[out.count][32] = 0;
        out.count++;
    }
    return out.count > 0;
}

bool health_parse(const char* body, size_t len, HealthStatus& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len)) return false;
    out.stt_ok = strcmp(doc["stt"] | "", "ok") == 0;
    out.llm_ok = strcmp(doc["llm"] | "", "ok") == 0;
    out.tts_ok = strcmp(doc["tts"] | "", "ok") == 0;
    return true;
}

#ifdef ARDUINO
#include <HTTPClient.h>
#include "secrets.h"
#include "util/log.h"

static bool http_get(const char* path, char* body, size_t body_cap, size_t* body_len) {
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", SERVER_HOST, SERVER_PORT, path);
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code != 200) {
        log_line(LOG_WARN, "net", "http_get_fail", "path=%s code=%d", path, code);
        http.end();
        return false;
    }
    String s = http.getString();
    size_t n = s.length();
    if (n > body_cap - 1) n = body_cap - 1;
    memcpy(body, s.c_str(), n);
    body[n] = 0;
    if (body_len) *body_len = n;
    http.end();
    return true;
}

bool styles_fetch(StyleList& out) {
    char body[2048];
    size_t n = 0;
    if (!http_get("/v1/styles", body, sizeof(body), &n)) return false;
    return styles_parse_list(body, n, out);
}

bool health_fetch(HealthStatus& out) {
    char body[256];
    size_t n = 0;
    if (!http_get("/healthz", body, sizeof(body), &n)) return false;
    return health_parse(body, n, out);
}
#endif
