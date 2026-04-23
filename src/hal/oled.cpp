#include "hal/oled.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>

namespace {
Adafruit_SSD1306 g_disp(128, 64, &Wire, -1);
}

bool oled_begin() {
    Wire.begin(41, 42);           // SDA=41, SCL=42
    Wire.setClock(400000);
    if (!g_disp.begin(SSD1306_SWITCHCAPVCC, 0x3C)) return false;
    g_disp.clearDisplay();
    g_disp.setTextColor(SSD1306_WHITE);
    g_disp.display();
    return true;
}
void oled_clear() { g_disp.clearDisplay(); }
void oled_show()  { g_disp.display(); }
void oled_text(int x, int y, uint8_t size, const char* s) {
    g_disp.setTextSize(size); g_disp.setCursor(x, y); g_disp.print(s);
}
void oled_rect(int x, int y, int w, int h, bool filled) {
    if (filled) g_disp.fillRect(x, y, w, h, SSD1306_WHITE);
    else        g_disp.drawRect(x, y, w, h, SSD1306_WHITE);
}
void oled_hbar(int x, int y, int w, int h, uint8_t fill) {
    g_disp.drawRect(x, y, w, h, SSD1306_WHITE);
    int inner = (w - 2) * fill / 255;
    g_disp.fillRect(x + 1, y + 1, inner, h - 2, SSD1306_WHITE);
}
