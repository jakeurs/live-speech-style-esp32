#pragma once
#include <cstdint>

enum class BtnId { WAKE, VOL_UP, VOL_DOWN };
enum class BtnEv { PRESS, RELEASE };

struct BtnEvent { BtnId id; BtnEv ev; };

void btns_begin();
bool btns_poll(BtnEvent& out);   // true if an event was returned
