#pragma once
#include <cstdint>
#include <cstddef>

void net_task_begin(int16_t* record_buf, size_t record_cap_bytes,
                    uint8_t* response_buf, size_t response_cap_bytes);
void net_begin_send(size_t captured_frames);
