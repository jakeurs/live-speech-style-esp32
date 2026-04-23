#include "net/response_parser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>

namespace {
const char* find_crlf(const char* s, const char* end) {
    for (const char* p = s; p + 1 < end; p++)
        if (p[0] == '\r' && p[1] == '\n') return p;
    return nullptr;
}

bool match_ci(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

void copy_trimmed(char* dst, size_t cap, const char* src, size_t n) {
    while (n && (src[0] == ' ' || src[0] == '\t')) { src++; n--; }
    while (n && (src[n-1] == ' ' || src[n-1] == '\t')) n--;
    size_t c = n < cap - 1 ? n : cap - 1;
    memcpy(dst, src, c);
    dst[c] = 0;
}
}

size_t resp_url_decode(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < cap; ) {
        if (in[i] == '%' && in[i+1] && in[i+2]) {
            char hex[3] = {in[i+1], in[i+2], 0};
            out[o++] = (char)strtol(hex, nullptr, 16);
            i += 3;
        } else if (in[i] == '+') {
            out[o++] = ' '; i++;
        } else {
            out[o++] = in[i++];
        }
    }
    out[o] = 0;
    return o;
}

bool resp_parse_headers(const char* raw, size_t len, RespHeaders& out) {
    const char* end = raw + len;
    const char* eol = find_crlf(raw, end);
    if (!eol) return false;
    if (sscanf(raw, "HTTP/%*d.%*d %d", &out.status) != 1) return false;
    const char* p = eol + 2;
    bool got_len = false;
    while (p < end) {
        const char* e = find_crlf(p, end);
        if (!e) break;
        if (e == p) break;
        const char* colon = (const char*)memchr(p, ':', e - p);
        if (colon) {
            size_t nlen = colon - p;
            const char* v = colon + 1;
            size_t vlen = e - v;
            if (nlen == 12 && match_ci(p, "Content-Type", 12)) {
                copy_trimmed(out.content_type, sizeof(out.content_type), v, vlen);
            } else if (nlen == 14 && match_ci(p, "Content-Length", 14)) {
                char buf[32]; copy_trimmed(buf, sizeof(buf), v, vlen);
                out.content_length = (uint32_t)strtoul(buf, nullptr, 10);
                got_len = true;
            } else if (nlen == 12 && match_ci(p, "X-Transcript", 12)) {
                char tmp[256]; copy_trimmed(tmp, sizeof(tmp), v, vlen);
                resp_url_decode(tmp, out.x_transcript, sizeof(out.x_transcript));
            } else if (nlen == 15 && match_ci(p, "X-Restyled-Text", 15)) {
                char tmp[256]; copy_trimmed(tmp, sizeof(tmp), v, vlen);
                resp_url_decode(tmp, out.x_restyled, sizeof(out.x_restyled));
            } else if (nlen == 12 && match_ci(p, "X-Request-Id", 12)) {
                copy_trimmed(out.x_request_id, sizeof(out.x_request_id), v, vlen);
            }
        }
        p = e + 2;
    }
    if (!got_len) return false;
    if (out.content_length > RESP_MAX_BODY) return false;
    return true;
}

bool resp_is_wav(const uint8_t r[12]) {
    return r[0]=='R' && r[1]=='I' && r[2]=='F' && r[3]=='F'
        && r[8]=='W' && r[9]=='A' && r[10]=='V' && r[11]=='E';
}
