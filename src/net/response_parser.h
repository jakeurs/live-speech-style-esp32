#pragma once
#include <cstdint>
#include <cstddef>

constexpr uint32_t RESP_MAX_BODY = 2 * 1024 * 1024;   // 2 MB cap

struct RespHeaders {
    int      status = 0;
    char     content_type[64] = {};
    uint32_t content_length = 0;
    char     x_transcript[256] = {};
    char     x_restyled[256] = {};
    char     x_request_id[64] = {};
};

bool resp_parse_headers(const char* raw, size_t len, RespHeaders& out);
bool resp_is_wav(const uint8_t riff12[12]);
size_t resp_url_decode(const char* in, char* out, size_t out_cap);
