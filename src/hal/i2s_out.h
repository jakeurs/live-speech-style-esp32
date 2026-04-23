#pragma once
#include <cstdint>
#include <cstddef>

using I2SOutFill = void (*)(int16_t* buf, size_t frames);

bool i2s_out_begin(I2SOutFill fill_cb);
void i2s_out_end();
