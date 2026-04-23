#pragma once
#include <cstdint>

struct NvsState {
    uint16_t style_idx = 0;
    char     style_id[33] = "jesus";
    uint8_t  volume_x10 = 6;
};

using NvsClock = uint32_t (*)();

NvsState nvs_load();
bool     nvs_save(const NvsState& s);
void     nvs_set_clock_ms(NvsClock clock);
void     nvs_test_reset();   // only for unit tests
