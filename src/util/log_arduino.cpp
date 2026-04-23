// Arduino glue: wires Serial as the log sink and millis() as the clock.
// This file is excluded from native builds via build_src_filter.
#include <Arduino.h>
#include "util/log.h"

static void serial_sink(const char* line) {
    Serial.println(line);
}

// Call once from setup() to attach the Serial sink and millis clock.
void log_init_arduino() {
    log_set_sink(serial_sink);
    log_set_clock_ms([]() -> uint32_t { return (uint32_t) millis(); });
}
