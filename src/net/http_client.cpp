#include "net/http_client.h"
#include "config/wav_header.h"
#include "util/log.h"
#include "secrets.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>
#include <Arduino.h>

namespace {

bool write_all(WiFiClient& c, const uint8_t* data, size_t n, uint32_t deadline_ms) {
    size_t sent = 0;
    while (sent < n) {
        if ((int32_t)(millis() - deadline_ms) > 0) return false;
        size_t w = c.write(data + sent, n - sent);
        if (w == 0) { delay(1); continue; }
        sent += w;
    }
    return true;
}

// Read bytes until a CRLFCRLF is observed (end of headers). Returns true on success.
// `buf` is filled with the headers (including trailing CRLFCRLF); `out` gets the length.
bool read_until_crlfcrlf(WiFiClient& c, char* buf, size_t cap, size_t& out,
                         uint32_t deadline_ms) {
    out = 0;
    while (out + 1 < cap) {
        if ((int32_t)(millis() - deadline_ms) > 0) return false;
        if (!c.available()) { delay(1); continue; }
        int b = c.read();
        if (b < 0) continue;
        buf[out++] = (char)b;
        if (out >= 4 && memcmp(buf + out - 4, "\r\n\r\n", 4) == 0) {
            buf[out] = 0;
            return true;
        }
    }
    return false;
}

}  // namespace

bool http_restyle(const RestyleRequest& req, RestyleResult& res, MilestoneCb cb) {
    const uint32_t t0 = millis();

    WiFiClient c;
    c.setTimeout(5);  // socket-level timeout (seconds)
    if (!c.connect(SERVER_HOST, SERVER_PORT)) {
        log_line(LOG_ERROR, "net", "connect_fail", "host=%s", SERVER_HOST);
        res.ok = false;
        return false;
    }
    res.t_connect_ms = millis() - t0;
    if (cb) cb(0);

    // Build boundary from the request_id (already a UUIDv4 string)
    char boundary[48];
    snprintf(boundary, sizeof(boundary), "XHS3E-%s", req.request_id);

    // Prepare WAV header for the PCM payload
    uint8_t wav_hdr[WAV_HEADER_SIZE];
    wav_header_build(wav_hdr, (uint32_t)req.pcm_byte_count);
    const uint32_t wav_total = WAV_HEADER_SIZE + (uint32_t)req.pcm_byte_count;

    // Multipart prologue: style_id part + start of audio part (up to the WAV bytes)
    char prologue[1024];
    int pn = snprintf(prologue, sizeof(prologue),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"style_id\"\r\n\r\n%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"utterance.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        boundary, req.style_id, boundary);

    // Multipart epilogue: language part + closing boundary
    char epilogue[512];
    int en = snprintf(epilogue, sizeof(epilogue),
        "\r\n--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n"
        "--%s--\r\n",
        boundary, req.language ? req.language : "en", boundary);

    const uint32_t body_len = (uint32_t)pn + wav_total + (uint32_t)en;

    // Request line + headers
    char req_line[512];
    int rn = snprintf(req_line, sizeof(req_line),
        "POST /v1/restyle HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: xh-s3e-ai/0.1\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %u\r\n"
        "X-Request-Id: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        SERVER_HOST, SERVER_PORT, boundary, (unsigned)body_len, req.request_id);

    const uint32_t upload_deadline = millis() + 10000;  // 10 s for upload

    if (!write_all(c, (const uint8_t*)req_line, (size_t)rn, upload_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    if (!write_all(c, (const uint8_t*)prologue, (size_t)pn, upload_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    if (!write_all(c, wav_hdr, WAV_HEADER_SIZE, upload_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    if (!write_all(c, req.pcm_bytes, req.pcm_byte_count, upload_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    if (!write_all(c, (const uint8_t*)epilogue, (size_t)en, upload_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    c.flush();
    res.t_upload_ms = millis() - t0;
    if (cb) cb(1);

    // Wait for headers (up to 45 s — the server's pipeline ceiling)
    char hdr[2048];
    size_t hdr_n = 0;
    const uint32_t first_byte_deadline = millis() + 45000;
    if (!read_until_crlfcrlf(c, hdr, sizeof(hdr), hdr_n, first_byte_deadline)) {
        c.stop(); res.ok = false; return false;
    }
    res.t_first_byte_ms = millis() - t0;
    if (cb) cb(2);

    if (!resp_parse_headers(hdr, hdr_n, res.headers)) {
        c.stop(); res.ok = false; return false;
    }

    // Body: read exactly content_length bytes, with a 10 s no-progress timeout
    size_t got = 0;
    uint32_t last_byte_ms = millis();
    while (got < res.headers.content_length) {
        if (millis() - last_byte_ms > 10000) {
            log_line(LOG_ERROR, "net", "dl_stall", "got=%u of=%u",
                     (unsigned)got, (unsigned)res.headers.content_length);
            c.stop(); res.ok = false; return false;
        }
        int b = c.read(res.response_buf + got,
                       res.headers.content_length - got);
        if (b > 0) { got += b; last_byte_ms = millis(); }
        else if (b == 0) { delay(1); }
        else {
            // -1 means socket closed before content_length was met.
            // Accept partial ONLY if the body is all-consumed; otherwise fail.
            break;
        }
    }
    res.response_len = got;
    c.stop();
    res.t_download_ms = millis() - t0;
    if (cb) cb(3);

    res.ok = (got == res.headers.content_length);
    return res.ok;
}
