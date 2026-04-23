#pragma once
#include <cstdint>
#include <cstdarg>

// Log severity levels
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
};

// Sink: called once per formatted log line (no trailing newline required from caller)
using LogSink    = void (*)(const char* line);
// Clock: returns milliseconds since boot
using LogClockFn = uint32_t (*)();

// Configuration (call before first use; safe to call from tests)
void log_set_sink(LogSink sink);
void log_set_clock_ms(LogClockFn clock_fn);

// Arduino convenience: wires Serial as sink and millis() as clock.
// Defined in log_arduino.cpp (excluded from native/test builds).
void log_init_arduino();

// Emit a log line:  [<ss.mmm>] LEVEL module event key=val ...
void log_line(LogLevel level, const char* module, const char* event,
              const char* fmt, ...);
