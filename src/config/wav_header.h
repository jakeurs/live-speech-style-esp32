#pragma once
#include <cstdint>
#include <cstddef>

constexpr uint32_t WAV_SAMPLE_RATE = 16000;
constexpr uint16_t WAV_CHANNELS    = 1;
constexpr uint16_t WAV_BITS        = 16;
constexpr size_t   WAV_HEADER_SIZE = 44;

void wav_header_build(uint8_t out[WAV_HEADER_SIZE], uint32_t pcm_bytes);
void wav_header_patch_length(uint8_t hdr[WAV_HEADER_SIZE], uint32_t pcm_bytes);
