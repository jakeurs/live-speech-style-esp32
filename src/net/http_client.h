#pragma once
#include <cstdint>
#include <cstddef>
#include "net/response_parser.h"

struct RestyleRequest {
    const char* style_id;
    const char* language;
    const uint8_t* pcm_bytes;      // record buffer contents
    size_t         pcm_byte_count;
    char           request_id[37]; // null-terminated UUIDv4
};

struct RestyleResult {
    RespHeaders headers;
    uint8_t*    response_buf;      // caller-owned, sized for RESP_MAX_BODY
    size_t      response_len;
    bool        ok;
    uint32_t    t_connect_ms;
    uint32_t    t_upload_ms;
    uint32_t    t_first_byte_ms;
    uint32_t    t_download_ms;
};

// Milestone codes emitted during the request lifecycle:
//   0 = TCP connected
//   1 = upload bytes fully flushed
//   2 = first response byte received
//   3 = body download complete
using MilestoneCb = void (*)(int milestone);

bool http_restyle(const RestyleRequest& req, RestyleResult& res, MilestoneCb cb);
