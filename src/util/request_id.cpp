#include "util/request_id.h"
#include <cstdio>

namespace {
uint32_t g_state = 0x12345678u;
uint32_t xorshift32() {
    uint32_t x = g_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_state = x;
    return x;
}
}

void request_id_seed(uint32_t seed) {
    g_state = seed ? seed : 0x12345678u;
}

void request_id_new(char out[37]) {
    uint8_t b[16];
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = xorshift32();
        b[i]   = (uint8_t)(r);
        b[i+1] = (uint8_t)(r >> 8);
        b[i+2] = (uint8_t)(r >> 16);
        b[i+3] = (uint8_t)(r >> 24);
    }
    b[6] = (b[6] & 0x0F) | 0x40;   // version 4
    b[8] = (b[8] & 0x3F) | 0x80;   // RFC 4122 variant
    snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],b[1],b[2],b[3], b[4],b[5], b[6],b[7], b[8],b[9],
        b[10],b[11],b[12],b[13],b[14],b[15]);
}
