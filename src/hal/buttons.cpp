#include "hal/buttons.h"
#include <Arduino.h>

namespace {
struct Btn { uint8_t pin; bool raw_last; bool stable; uint32_t change_ms; BtnId id; };
Btn g_btns[3] = {
    {0,  true, true, 0, BtnId::WAKE},
    {40, true, true, 0, BtnId::VOL_UP},
    {39, true, true, 0, BtnId::VOL_DOWN},
};
constexpr uint32_t DEBOUNCE_MS = 20;
BtnEvent g_q[8];
volatile uint8_t g_head = 0, g_tail = 0;
bool push(BtnEvent e) {
    uint8_t n = (g_head + 1) & 7;
    if (n == g_tail) return false;
    g_q[g_head] = e; g_head = n; return true;
}
}

void btns_begin() {
    for (auto& b : g_btns) pinMode(b.pin, INPUT_PULLUP);
}

bool btns_poll(BtnEvent& out) {
    uint32_t now = millis();
    for (auto& b : g_btns) {
        bool raw = digitalRead(b.pin);  // active low; true = released
        if (raw != b.raw_last) { b.raw_last = raw; b.change_ms = now; }
        if (raw != b.stable && (now - b.change_ms) > DEBOUNCE_MS) {
            b.stable = raw;
            push({b.id, raw ? BtnEv::RELEASE : BtnEv::PRESS});
        }
    }
    if (g_tail == g_head) return false;
    out = g_q[g_tail]; g_tail = (g_tail + 1) & 7;
    return true;
}
