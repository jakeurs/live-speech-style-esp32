#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>

struct I2SInCtx {
    int16_t* buf;
    size_t   capacity_frames;
    volatile size_t write_pos_frames;
    std::atomic<uint16_t> last_rms;   // 0..65535
};

bool i2s_in_begin(I2SInCtx* ctx);
void i2s_in_stop();
