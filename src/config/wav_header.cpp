#include "config/wav_header.h"
#include <cstring>

namespace {
void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
void put_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
}
}

void wav_header_build(uint8_t o[44], uint32_t pcm_bytes) {
    memcpy(o + 0,  "RIFF", 4);
    put_le32(o + 4,  pcm_bytes + 36);
    memcpy(o + 8,  "WAVE", 4);
    memcpy(o + 12, "fmt ", 4);
    put_le32(o + 16, 16);
    put_le16(o + 20, 1);
    put_le16(o + 22, WAV_CHANNELS);
    put_le32(o + 24, WAV_SAMPLE_RATE);
    put_le32(o + 28, WAV_SAMPLE_RATE * WAV_CHANNELS * (WAV_BITS / 8));
    put_le16(o + 32, WAV_CHANNELS * (WAV_BITS / 8));
    put_le16(o + 34, WAV_BITS);
    memcpy(o + 36, "data", 4);
    put_le32(o + 40, pcm_bytes);
}

void wav_header_patch_length(uint8_t h[44], uint32_t pcm_bytes) {
    put_le32(h + 4,  pcm_bytes + 36);
    put_le32(h + 40, pcm_bytes);
}
