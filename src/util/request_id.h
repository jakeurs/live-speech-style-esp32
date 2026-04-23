#pragma once
#include <cstdint>

void request_id_seed(uint32_t seed);
void request_id_new(char out[37]);   // writes 36 chars + NUL
