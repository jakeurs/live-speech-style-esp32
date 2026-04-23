#include "util/log.h"
#include <cstdio>
#include <cstring>

// ---- module-private state --------------------------------------------------

static LogSink    s_sink     = nullptr;
static LogClockFn s_clock_fn = nullptr;

// ---- configuration ---------------------------------------------------------

void log_set_sink(LogSink sink) {
    s_sink = sink;
}

void log_set_clock_ms(LogClockFn clock_fn) {
    s_clock_fn = clock_fn;
}

// ---- formatting helpers ----------------------------------------------------

static const char* level_str(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default:        return "?";
    }
}

// ---- public API ------------------------------------------------------------

void log_line(LogLevel level, const char* module, const char* event,
              const char* fmt, ...) {
    if (!s_sink) return;

    // Timestamp: "[<ssss.mmm>]" — seconds.milliseconds, right-justified
    char ts_buf[16] = "[   0.000]";
    if (s_clock_fn) {
        uint32_t ms  = s_clock_fn();
        uint32_t sec = ms / 1000u;
        uint32_t frac = ms % 1000u;
        // Format as fixed-width: 4-digit seconds, 3-digit millis
        snprintf(ts_buf, sizeof(ts_buf), "[%4u.%03u]", sec, frac);
    }

    // Format the payload
    char payload[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(payload, sizeof(payload), fmt, args);
    va_end(args);

    // Assemble: "[  12.345] INFO net upload_done bytes=491520 rid=abc"
    char line[512];
    snprintf(line, sizeof(line), "%s %s %s %s %s",
             ts_buf, level_str(level), module, event, payload);

    s_sink(line);
}
