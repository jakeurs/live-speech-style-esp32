#pragma once
#include <cstdint>

bool oled_begin();
void oled_clear();
void oled_show();
void oled_text(int x, int y, uint8_t size, const char* s);
void oled_rect(int x, int y, int w, int h, bool filled);
void oled_hbar(int x, int y, int w, int h, uint8_t fill_fraction_0_255);
