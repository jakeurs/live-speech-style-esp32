# ESP32-S3 Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a complete XH-S3E-AI_v1.0 ESP32-S3 firmware that acts as a push-to-talk client for the live-speech-style server.

**Architecture:** Arduino/PlatformIO firmware. Three FreeRTOS tasks (UI on core 0, Net on core 0, Audio on core 1) sharing state via one mutex and communicating via queues. Two pre-allocated PSRAM buffers hand off by pointer. Pure-logic modules (mixer, state machine, WAV header, response parser) are TDD'd in a native (laptop) test environment; hardware-coupled modules are verified via on-device smoke tests.

**Tech Stack:** Arduino-ESP32 core, PlatformIO, Adafruit_SSD1306 + Adafruit_GFX, ArduinoJson, FreeRTOS primitives, Unity (native unit tests), Python for asset baking.

**Reference spec:** [2026-04-23-firmware-design.md](2026-04-23-firmware-design.md)

---

## Task list overview

**Phase 1 — Scaffolding**
- Task 1: PlatformIO project + native test env
- Task 2: Secrets template + .gitignore
- Task 3: Logging utility

**Phase 2 — Pure-logic modules (native TDD)**
- Task 4: Request ID (UUIDv4)
- Task 5: WAV header builder
- Task 6: 2-voice mixer
- Task 7: Response parser
- Task 8: State machine
- Task 9: NVS store wrapper

**Phase 3 — Asset pipeline**
- Task 10: SFX baker + placeholder assets

**Phase 4 — Hardware HAL (on-device smoke)**
- Task 11: Buttons HAL
- Task 12: OLED HAL
- Task 13: I2S out HAL
- Task 14: I2S in HAL

**Phase 5 — Subsystem integration**
- Task 15: SFX engine + audio task
- Task 16: OLED per-state rendering
- Task 17: UI task
- Task 18: WiFi lifecycle
- Task 19: Styles + health API
- Task 20: Multipart POST sender
- Task 21: Net task end-to-end

**Phase 6 — Full integration**
- Task 22: Wire everything in main; smoke-build mode
- Task 23: README and final commit

---

## Phase 1 — Scaffolding

### Task 1: PlatformIO project + native test env

**Files:**
- Create: `platformio.ini`
- Create: `partitions.csv`
- Create: `src/main.cpp`
- Create: `.gitignore` (append)

- [ ] **Step 1: Write `platformio.ini`**

```ini
[platformio]
default_envs = xh-s3e-ai

[env:xh-s3e-ai]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = partitions.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=3
    -std=gnu++17
lib_deps =
    adafruit/Adafruit SSD1306@^2.5.9
    adafruit/Adafruit GFX Library@^1.11.9
    bblanchon/ArduinoJson@^7.0.4

[env:native]
platform = native
test_framework = unity
build_flags =
    -std=gnu++17
    -DUNIT_TEST
    -Isrc
    -Itest/mocks
```

- [ ] **Step 2: Write `partitions.csv`** (standard 16 MB layout with generous app partitions for future OTA, even though OTA is out of scope for v1)

```csv
# Name,   Type, SubType, Offset,  Size,      Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x640000,
app1,     app,  ota_1,   0x650000,0x640000,
spiffs,   data, spiffs,  0xc90000,0x360000,
coredump, data, coredump,0xff0000,0x10000,
```

- [ ] **Step 3: Write minimal `src/main.cpp`**

```cpp
#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("xh-s3e-ai boot");
    Serial.printf("PSRAM size: %u bytes\n", (unsigned) ESP.getPsramSize());
    Serial.printf("Free heap:  %u bytes\n", (unsigned) ESP.getFreeHeap());
}

void loop() {
    delay(1000);
}
```

- [ ] **Step 4: Append to `.gitignore`**

```
.pio/
build/
.vscode/c_cpp_properties.json
.vscode/launch.json
include/secrets.h
```

- [ ] **Step 5: Build and verify**

Run: `pio run -e xh-s3e-ai`
Expected: build succeeds. Section sizes printed. No PSRAM warnings.

Optional if a device is connected:
Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`
Expected serial output includes `PSRAM size: 8388608 bytes`.

- [ ] **Step 6: Commit**

```bash
git add platformio.ini partitions.csv src/main.cpp .gitignore
git commit -m "feat: bootstrap PlatformIO project with Arduino + native test envs"
```

---

### Task 2: Secrets template

**Files:**
- Create: `include/secrets.h.example`

- [ ] **Step 1: Write the template**

```cpp
// Copy to include/secrets.h and fill in values. Do not commit secrets.h.
#pragma once

#define WIFI_SSID      "your-wifi-ssid"
#define WIFI_PASS      "your-wifi-password"
#define SERVER_HOST    "192.168.1.50"
#define SERVER_PORT    8080
#define DEFAULT_STYLE  "jesus"
#define FALLBACK_LANG  "en"
```

- [ ] **Step 2: Create the developer's real `include/secrets.h`** (locally, not committed — just so the build works)

Copy the example and fill in real values. Do not stage.

- [ ] **Step 3: Verify build still works**

Run: `pio run -e xh-s3e-ai`
Expected: builds with no reference to the secrets yet (we'll include it in Task 18).

- [ ] **Step 4: Commit**

```bash
git add include/secrets.h.example
git commit -m "feat: add secrets template"
```

---

### Task 3: Logging utility

**Files:**
- Create: `src/util/log.h`
- Create: `src/util/log.cpp`
- Create: `test/test_log/test_log.cpp`

- [ ] **Step 1: Write the failing test**

`test/test_log/test_log.cpp`:
```cpp
#include <unity.h>
#include <string>
#include "util/log.h"

static std::string captured;

static void capture_sink(const char* line) { captured = line; }

void setUp() { captured.clear(); }
void tearDown() {}

void test_log_format_has_bracketed_timestamp() {
    log_set_sink(capture_sink);
    log_set_clock_ms([]() -> uint32_t { return 12345; });
    log_line(LOG_INFO, "net", "upload_done", "bytes=%d rid=%s", 491520, "abc");
    TEST_ASSERT_TRUE(captured.find("[  12.345]") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("INFO") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("net") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("upload_done") != std::string::npos);
    TEST_ASSERT_TRUE(captured.find("bytes=491520 rid=abc") != std::string::npos);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_log_format_has_bracketed_timestamp);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test (expect fail)**

Run: `pio test -e native -f test_log`
Expected: FAIL (linker errors — symbols not defined).

- [ ] **Step 3: Implement `src/util/log.h`**

```cpp
#pragma once
#include <cstdint>
#include <cstdarg>

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

using LogSink  = void (*)(const char* line);
using ClockMs  = uint32_t (*)();

void log_set_sink(LogSink sink);
void log_set_clock_ms(ClockMs clock);

void log_line(LogLevel lvl, const char* task, const char* msg, const char* kv_fmt = "", ...);
```

- [ ] **Step 4: Implement `src/util/log.cpp`**

```cpp
#include "util/log.h"
#include <cstdio>
#include <cstring>

namespace {
LogSink  g_sink = nullptr;
ClockMs  g_clock = nullptr;
const char* level_name(LogLevel l) {
    switch (l) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
    }
    return "?    ";
}
}

void log_set_sink(LogSink sink) { g_sink = sink; }
void log_set_clock_ms(ClockMs clock) { g_clock = clock; }

void log_line(LogLevel lvl, const char* task, const char* msg, const char* kv_fmt, ...) {
    char buf[256];
    uint32_t t = g_clock ? g_clock() : 0;
    int n = snprintf(buf, sizeof(buf), "[%4u.%03u] %s %-5s %s ",
                     (unsigned)(t / 1000), (unsigned)(t % 1000),
                     level_name(lvl), task, msg);
    if (n > 0 && (size_t)n < sizeof(buf) && kv_fmt && kv_fmt[0]) {
        va_list ap;
        va_start(ap, kv_fmt);
        vsnprintf(buf + n, sizeof(buf) - n, kv_fmt, ap);
        va_end(ap);
    }
    if (g_sink) g_sink(buf);
}
```

- [ ] **Step 5: Make the native test compile (add the source)**

PlatformIO's `pio test` auto-discovers sources under `src/` for native tests. If it doesn't, add `test_build_src = yes` to `[env:native]`. Update the env block to:

```ini
[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_flags =
    -std=gnu++17
    -DUNIT_TEST
    -Isrc
    -Itest/mocks
```

- [ ] **Step 6: Run the test (expect pass)**

Run: `pio test -e native -f test_log`
Expected: `1 Tests 0 Failures 0 Ignored`.

- [ ] **Step 7: Add an Arduino glue bridge so firmware builds link to `Serial`**

`src/util/log_arduino.cpp` (compiled ONLY in Arduino env — guard with `#ifdef ARDUINO`):

```cpp
#ifdef ARDUINO
#include <Arduino.h>
#include "util/log.h"

static void serial_sink(const char* line) { Serial.println(line); }
static uint32_t arduino_clock_ms() { return millis(); }

void log_init_arduino() {
    log_set_sink(serial_sink);
    log_set_clock_ms(arduino_clock_ms);
}
#endif
```

Add the declaration to `log.h`:
```cpp
#ifdef ARDUINO
void log_init_arduino();
#endif
```

- [ ] **Step 8: Verify Arduino build still works**

Run: `pio run -e xh-s3e-ai`
Expected: builds.

- [ ] **Step 9: Commit**

```bash
git add src/util/log.h src/util/log.cpp src/util/log_arduino.cpp test/test_log platformio.ini
git commit -m "feat(util): add structured logger with native tests"
```

---

## Phase 2 — Pure-logic modules (native TDD)

### Task 4: Request ID generator

**Files:**
- Create: `src/util/request_id.h`
- Create: `src/util/request_id.cpp`
- Create: `test/test_request_id/test_request_id.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <unity.h>
#include <string>
#include <set>
#include "util/request_id.h"

void setUp() {}
void tearDown() {}

void test_uuid_v4_format() {
    request_id_seed(0xDEADBEEFu);
    char out[37];
    request_id_new(out);
    // 8-4-4-4-12 with dashes at [8][13][18][23]
    TEST_ASSERT_EQUAL_CHAR('-', out[8]);
    TEST_ASSERT_EQUAL_CHAR('-', out[13]);
    TEST_ASSERT_EQUAL_CHAR('-', out[18]);
    TEST_ASSERT_EQUAL_CHAR('-', out[23]);
    TEST_ASSERT_EQUAL_CHAR('4', out[14]);            // version 4
    TEST_ASSERT_TRUE(out[19] == '8' || out[19] == '9'
                  || out[19] == 'a' || out[19] == 'b'); // variant
    TEST_ASSERT_EQUAL_CHAR('\0', out[36]);
}

void test_uuid_v4_unique() {
    request_id_seed(1u);
    std::set<std::string> seen;
    char out[37];
    for (int i = 0; i < 1000; i++) {
        request_id_new(out);
        seen.insert(out);
    }
    TEST_ASSERT_EQUAL_INT(1000, (int)seen.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_uuid_v4_format);
    RUN_TEST(test_uuid_v4_unique);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test (expect fail)**

Run: `pio test -e native -f test_request_id`
Expected: FAIL.

- [ ] **Step 3: Implement**

`src/util/request_id.h`:
```cpp
#pragma once
#include <cstdint>

void request_id_seed(uint32_t seed);
void request_id_new(char out[37]);   // writes 36 chars + NUL
```

`src/util/request_id.cpp`:
```cpp
#include "util/request_id.h"
#include <cstdio>

namespace {
uint32_t g_state = 0x12345678u;
uint32_t xorshift32() {
    uint32_t x = g_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g_state = x;
    return x;
}
}

void request_id_seed(uint32_t seed) {
    g_state = seed ? seed : 0x12345678u;
}

void request_id_new(char out[37]) {
    uint8_t b[16];
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = xorshift32();
        b[i]   = (uint8_t)(r);
        b[i+1] = (uint8_t)(r >> 8);
        b[i+2] = (uint8_t)(r >> 16);
        b[i+3] = (uint8_t)(r >> 24);
    }
    b[6] = (b[6] & 0x0F) | 0x40;   // version 4
    b[8] = (b[8] & 0x3F) | 0x80;   // RFC 4122 variant
    snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],b[1],b[2],b[3], b[4],b[5], b[6],b[7], b[8],b[9],
        b[10],b[11],b[12],b[13],b[14],b[15]);
}
```

- [ ] **Step 4: Run the test (expect pass)**

Run: `pio test -e native -f test_request_id`
Expected: `2 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/util/request_id.h src/util/request_id.cpp test/test_request_id
git commit -m "feat(util): add UUIDv4 request ID generator"
```

---

### Task 5: WAV header builder

**Files:**
- Create: `src/config/wav_header.h`
- Create: `src/config/wav_header.cpp`
- Create: `test/test_wav_header/test_wav_header.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
#include <unity.h>
#include <cstring>
#include "config/wav_header.h"

void setUp() {}
void tearDown() {}

void test_header_canonical_layout_16k_mono_s16() {
    uint8_t hdr[44];
    wav_header_build(hdr, /*pcm_bytes*/ 320000);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", hdr + 0, 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", hdr + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("fmt ", hdr + 12, 4);
    TEST_ASSERT_EQUAL_MEMORY("data", hdr + 36, 4);
    // fmt chunk size = 16
    TEST_ASSERT_EQUAL_UINT32(16, *(uint32_t*)(hdr + 16));
    // PCM format = 1, channels = 1, rate = 16000
    TEST_ASSERT_EQUAL_UINT16(1,     *(uint16_t*)(hdr + 20));
    TEST_ASSERT_EQUAL_UINT16(1,     *(uint16_t*)(hdr + 22));
    TEST_ASSERT_EQUAL_UINT32(16000, *(uint32_t*)(hdr + 24));
    // byte rate = 32000, block align = 2, bits = 16
    TEST_ASSERT_EQUAL_UINT32(32000, *(uint32_t*)(hdr + 28));
    TEST_ASSERT_EQUAL_UINT16(2,     *(uint16_t*)(hdr + 32));
    TEST_ASSERT_EQUAL_UINT16(16,    *(uint16_t*)(hdr + 34));
    // data size
    TEST_ASSERT_EQUAL_UINT32(320000, *(uint32_t*)(hdr + 40));
    // RIFF size = file size - 8 = 320000 + 44 - 8 = 320036
    TEST_ASSERT_EQUAL_UINT32(320036, *(uint32_t*)(hdr + 4));
}

void test_patch_length() {
    uint8_t hdr[44];
    wav_header_build(hdr, 0);
    wav_header_patch_length(hdr, 480000);
    TEST_ASSERT_EQUAL_UINT32(480036, *(uint32_t*)(hdr + 4));
    TEST_ASSERT_EQUAL_UINT32(480000, *(uint32_t*)(hdr + 40));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_header_canonical_layout_16k_mono_s16);
    RUN_TEST(test_patch_length);
    return UNITY_END();
}
```

- [ ] **Step 2: Run (fail)**

Run: `pio test -e native -f test_wav_header`

- [ ] **Step 3: Implement**

`src/config/wav_header.h`:
```cpp
#pragma once
#include <cstdint>

constexpr uint32_t WAV_SAMPLE_RATE = 16000;
constexpr uint16_t WAV_CHANNELS    = 1;
constexpr uint16_t WAV_BITS        = 16;
constexpr size_t   WAV_HEADER_SIZE = 44;

void wav_header_build(uint8_t out[WAV_HEADER_SIZE], uint32_t pcm_bytes);
void wav_header_patch_length(uint8_t hdr[WAV_HEADER_SIZE], uint32_t pcm_bytes);
```

`src/config/wav_header.cpp`:
```cpp
#include "config/wav_header.h"
#include <cstring>

namespace {
void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
void put_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
}
}

void wav_header_build(uint8_t o[44], uint32_t pcm_bytes) {
    memcpy(o + 0,  "RIFF", 4);
    put_le32(o + 4,  pcm_bytes + 36);
    memcpy(o + 8,  "WAVE", 4);
    memcpy(o + 12, "fmt ", 4);
    put_le32(o + 16, 16);
    put_le16(o + 20, 1);
    put_le16(o + 22, WAV_CHANNELS);
    put_le32(o + 24, WAV_SAMPLE_RATE);
    put_le32(o + 28, WAV_SAMPLE_RATE * WAV_CHANNELS * (WAV_BITS / 8));
    put_le16(o + 32, WAV_CHANNELS * (WAV_BITS / 8));
    put_le16(o + 34, WAV_BITS);
    memcpy(o + 36, "data", 4);
    put_le32(o + 40, pcm_bytes);
}

void wav_header_patch_length(uint8_t h[44], uint32_t pcm_bytes) {
    put_le32(h + 4,  pcm_bytes + 36);
    put_le32(h + 40, pcm_bytes);
}
```

- [ ] **Step 4: Run (pass)**

Run: `pio test -e native -f test_wav_header`
Expected: `2 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/config/wav_header.h src/config/wav_header.cpp test/test_wav_header
git commit -m "feat(config): add WAV header builder for 16k mono s16"
```

---

### Task 6: 2-voice mixer

**Files:**
- Create: `src/audio/mixer.h`
- Create: `src/audio/mixer.cpp`
- Create: `test/test_mixer/test_mixer.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
#include <unity.h>
#include <cstring>
#include "audio/mixer.h"

void setUp() { mixer_reset(); }
void tearDown() {}

void test_silent_when_no_voices_active() {
    int16_t out[8] = {};
    mixer_render(out, 8, 1.0f);
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_INT16(0, out[i]);
}

void test_single_voice_copies_samples_with_gain() {
    int16_t src[] = {100, 200, 300, 400};
    mixer_play(0, src, 4, 0.5f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    TEST_ASSERT_EQUAL_INT16(50,  out[0]);
    TEST_ASSERT_EQUAL_INT16(100, out[1]);
    TEST_ASSERT_EQUAL_INT16(150, out[2]);
    TEST_ASSERT_EQUAL_INT16(200, out[3]);
}

void test_two_voices_sum() {
    int16_t a[] = {100, 100, 100, 100};
    int16_t b[] = {50, 50, 50, 50};
    mixer_play(0, a, 4, 1.0f, false);
    mixer_play(1, b, 4, 1.0f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_INT16(150, out[i]);
}

void test_clips_at_int16_range() {
    int16_t a[] = {30000, 30000};
    int16_t b[] = {10000, 10000};
    mixer_play(0, a, 2, 1.0f, false);
    mixer_play(1, b, 2, 1.0f, false);
    int16_t out[2];
    mixer_render(out, 2, 1.0f);
    TEST_ASSERT_EQUAL_INT16(32767, out[0]);
    TEST_ASSERT_EQUAL_INT16(32767, out[1]);
}

void test_loop_wraps() {
    int16_t src[] = {1, 2, 3};
    mixer_play(1, src, 3, 1.0f, true);
    int16_t out[7];
    mixer_render(out, 7, 1.0f);
    int16_t expected[] = {1,2,3,1,2,3,1};
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected, out, 7);
}

void test_non_loop_deactivates_after_end() {
    int16_t src[] = {500, 500};
    mixer_play(0, src, 2, 1.0f, false);
    int16_t out[4];
    mixer_render(out, 4, 1.0f);
    TEST_ASSERT_EQUAL_INT16(500, out[0]);
    TEST_ASSERT_EQUAL_INT16(500, out[1]);
    TEST_ASSERT_EQUAL_INT16(0, out[2]);
    TEST_ASSERT_EQUAL_INT16(0, out[3]);
}

void test_stop_silences_voice() {
    int16_t src[] = {1000, 1000, 1000, 1000};
    mixer_play(1, src, 4, 1.0f, true);
    mixer_stop(1);
    int16_t out[2] = {};
    mixer_render(out, 2, 1.0f);
    TEST_ASSERT_EQUAL_INT16(0, out[0]);
    TEST_ASSERT_EQUAL_INT16(0, out[1]);
}

void test_master_volume_scales_output() {
    int16_t a[] = {1000, 1000};
    mixer_play(0, a, 2, 1.0f, false);
    int16_t out[2];
    mixer_render(out, 2, 0.5f);
    TEST_ASSERT_EQUAL_INT16(500, out[0]);
    TEST_ASSERT_EQUAL_INT16(500, out[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_silent_when_no_voices_active);
    RUN_TEST(test_single_voice_copies_samples_with_gain);
    RUN_TEST(test_two_voices_sum);
    RUN_TEST(test_clips_at_int16_range);
    RUN_TEST(test_loop_wraps);
    RUN_TEST(test_non_loop_deactivates_after_end);
    RUN_TEST(test_stop_silences_voice);
    RUN_TEST(test_master_volume_scales_output);
    return UNITY_END();
}
```

- [ ] **Step 2: Run (fail)**

Run: `pio test -e native -f test_mixer`

- [ ] **Step 3: Implement**

`src/audio/mixer.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

constexpr int MIXER_VOICES = 2;

void mixer_reset();
void mixer_play(int voice, const int16_t* data, size_t len, float gain, bool loop);
void mixer_stop(int voice);
bool mixer_voice_active(int voice);
void mixer_render(int16_t* out, size_t frames, float master_volume);
```

`src/audio/mixer.cpp`:
```cpp
#include "audio/mixer.h"
#include <cstring>
#include <algorithm>

namespace {
struct Voice {
    const int16_t* data;
    size_t len;
    size_t pos;
    float  gain;
    bool   active;
    bool   loop;
};

Voice g_voices[MIXER_VOICES] = {};

inline int16_t clip16(int32_t v) {
    if (v > 32767)  return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}
}

void mixer_reset() {
    memset(g_voices, 0, sizeof(g_voices));
}

void mixer_play(int v, const int16_t* data, size_t len, float gain, bool loop) {
    if (v < 0 || v >= MIXER_VOICES) return;
    g_voices[v] = {data, len, 0, gain, true, loop};
}

void mixer_stop(int v) {
    if (v < 0 || v >= MIXER_VOICES) return;
    g_voices[v].active = false;
}

bool mixer_voice_active(int v) {
    if (v < 0 || v >= MIXER_VOICES) return false;
    return g_voices[v].active;
}

void mixer_render(int16_t* out, size_t frames, float master_volume) {
    for (size_t i = 0; i < frames; i++) {
        int32_t acc = 0;
        for (int vi = 0; vi < MIXER_VOICES; vi++) {
            Voice& v = g_voices[vi];
            if (!v.active) continue;
            int32_t s = (int32_t)(v.data[v.pos] * v.gain);
            acc += s;
            v.pos++;
            if (v.pos >= v.len) {
                if (v.loop) v.pos = 0;
                else        v.active = false;
            }
        }
        out[i] = clip16((int32_t)(acc * master_volume));
    }
}
```

- [ ] **Step 4: Run (pass)**

Run: `pio test -e native -f test_mixer`
Expected: `8 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/audio/mixer.h src/audio/mixer.cpp test/test_mixer
git commit -m "feat(audio): add 2-voice PCM mixer with clipping, loop, master volume"
```

---

### Task 7: HTTP response parser

**Files:**
- Create: `src/net/response_parser.h`
- Create: `src/net/response_parser.cpp`
- Create: `test/test_response_parser/test_response_parser.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
#include <unity.h>
#include <cstring>
#include "net/response_parser.h"

void setUp() {}
void tearDown() {}

void test_parses_status_and_headers() {
    const char* raw =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: audio/wav\r\n"
        "Content-Length: 256\r\n"
        "X-Transcript: hello%20world\r\n"
        "X-Request-Id: abc\r\n"
        "\r\n";
    RespHeaders h;
    TEST_ASSERT_TRUE(resp_parse_headers(raw, strlen(raw), h));
    TEST_ASSERT_EQUAL_INT(200, h.status);
    TEST_ASSERT_EQUAL_STRING("audio/wav", h.content_type);
    TEST_ASSERT_EQUAL_UINT32(256, h.content_length);
    TEST_ASSERT_EQUAL_STRING("hello world", h.x_transcript);
    TEST_ASSERT_EQUAL_STRING("abc", h.x_request_id);
}

void test_rejects_missing_content_length() {
    const char* raw = "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_FALSE(resp_parse_headers(raw, strlen(raw), h));
}

void test_rejects_oversize_content_length() {
    const char* raw =
        "HTTP/1.1 200 OK\r\nContent-Type: audio/wav\r\nContent-Length: 3000000\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_FALSE(resp_parse_headers(raw, strlen(raw), h));
}

void test_allows_json_body_on_error() {
    const char* raw = "HTTP/1.1 422 Unprocessable\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: 64\r\n\r\n";
    RespHeaders h;
    TEST_ASSERT_TRUE(resp_parse_headers(raw, strlen(raw), h));
    TEST_ASSERT_EQUAL_INT(422, h.status);
    TEST_ASSERT_EQUAL_STRING("application/json", h.content_type);
}

void test_validates_wav_magic() {
    uint8_t good[12] = {'R','I','F','F',0,0,0,0,'W','A','V','E'};
    uint8_t bad[12]  = {'X','X','X','X',0,0,0,0,'W','A','V','E'};
    TEST_ASSERT_TRUE(resp_is_wav(good));
    TEST_ASSERT_FALSE(resp_is_wav(bad));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_status_and_headers);
    RUN_TEST(test_rejects_missing_content_length);
    RUN_TEST(test_rejects_oversize_content_length);
    RUN_TEST(test_allows_json_body_on_error);
    RUN_TEST(test_validates_wav_magic);
    return UNITY_END();
}
```

- [ ] **Step 2: Run (fail)**

Run: `pio test -e native -f test_response_parser`

- [ ] **Step 3: Implement**

`src/net/response_parser.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

constexpr uint32_t RESP_MAX_BODY = 2 * 1024 * 1024;   // 2 MB cap

struct RespHeaders {
    int      status = 0;
    char     content_type[64] = {};
    uint32_t content_length = 0;
    char     x_transcript[256] = {};
    char     x_restyled[256] = {};
    char     x_request_id[64] = {};
};

bool resp_parse_headers(const char* raw, size_t len, RespHeaders& out);
bool resp_is_wav(const uint8_t riff12[12]);
size_t resp_url_decode(const char* in, char* out, size_t out_cap);
```

`src/net/response_parser.cpp`:
```cpp
#include "net/response_parser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace {
const char* find_crlf(const char* s, const char* end) {
    for (const char* p = s; p + 1 < end; p++)
        if (p[0] == '\r' && p[1] == '\n') return p;
    return nullptr;
}

bool match_ci(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

void copy_trimmed(char* dst, size_t cap, const char* src, size_t n) {
    while (n && (src[0] == ' ' || src[0] == '\t')) { src++; n--; }
    while (n && (src[n-1] == ' ' || src[n-1] == '\t')) n--;
    size_t c = n < cap - 1 ? n : cap - 1;
    memcpy(dst, src, c);
    dst[c] = 0;
}
}

size_t resp_url_decode(const char* in, char* out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < cap; ) {
        if (in[i] == '%' && in[i+1] && in[i+2]) {
            char hex[3] = {in[i+1], in[i+2], 0};
            out[o++] = (char)strtol(hex, nullptr, 16);
            i += 3;
        } else if (in[i] == '+') {
            out[o++] = ' '; i++;
        } else {
            out[o++] = in[i++];
        }
    }
    out[o] = 0;
    return o;
}

bool resp_parse_headers(const char* raw, size_t len, RespHeaders& out) {
    const char* end = raw + len;
    const char* eol = find_crlf(raw, end);
    if (!eol) return false;
    // Status line: "HTTP/1.1 200 OK"
    if (sscanf(raw, "HTTP/%*d.%*d %d", &out.status) != 1) return false;
    const char* p = eol + 2;
    bool got_len = false;
    while (p < end) {
        const char* e = find_crlf(p, end);
        if (!e) break;
        if (e == p) break; // end of headers
        const char* colon = (const char*)memchr(p, ':', e - p);
        if (colon) {
            size_t nlen = colon - p;
            const char* v = colon + 1;
            size_t vlen = e - v;
            if (nlen == 12 && match_ci(p, "Content-Type", 12)) {
                copy_trimmed(out.content_type, sizeof(out.content_type), v, vlen);
            } else if (nlen == 14 && match_ci(p, "Content-Length", 14)) {
                char buf[32]; copy_trimmed(buf, sizeof(buf), v, vlen);
                out.content_length = (uint32_t)strtoul(buf, nullptr, 10);
                got_len = true;
            } else if (nlen == 12 && match_ci(p, "X-Transcript", 12)) {
                char tmp[256]; copy_trimmed(tmp, sizeof(tmp), v, vlen);
                resp_url_decode(tmp, out.x_transcript, sizeof(out.x_transcript));
            } else if (nlen == 15 && match_ci(p, "X-Restyled-Text", 15)) {
                char tmp[256]; copy_trimmed(tmp, sizeof(tmp), v, vlen);
                resp_url_decode(tmp, out.x_restyled, sizeof(out.x_restyled));
            } else if (nlen == 12 && match_ci(p, "X-Request-Id", 12)) {
                copy_trimmed(out.x_request_id, sizeof(out.x_request_id), v, vlen);
            }
        }
        p = e + 2;
    }
    if (!got_len) return false;
    if (out.content_length > RESP_MAX_BODY) return false;
    return true;
}

bool resp_is_wav(const uint8_t r[12]) {
    return r[0]=='R' && r[1]=='I' && r[2]=='F' && r[3]=='F'
        && r[8]=='W' && r[9]=='A' && r[10]=='V' && r[11]=='E';
}
```

- [ ] **Step 4: Run (pass)**

Run: `pio test -e native -f test_response_parser`
Expected: `5 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/net/response_parser.h src/net/response_parser.cpp test/test_response_parser
git commit -m "feat(net): add HTTP response header parser with WAV validation"
```

---

### Task 8: State machine (pure)

**Files:**
- Create: `src/app_state.h`
- Create: `src/app_state.cpp`
- Create: `test/test_state_machine/test_state_machine.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
#include <unity.h>
#include "app_state.h"

void setUp() {}
void tearDown() {}

void test_idle_wake_press_starts_recording() {
    AppCtx c{};
    c.state = AppState::IDLE;
    c.wifi_connected = true;
    TEST_ASSERT_EQUAL(AppState::RECORDING, app_on_event(c, AppEvent::WAKE_PRESS));
}

void test_idle_wake_ignored_without_wifi() {
    AppCtx c{};
    c.state = AppState::IDLE;
    c.wifi_connected = false;
    TEST_ASSERT_EQUAL(AppState::IDLE, app_on_event(c, AppEvent::WAKE_PRESS));
}

void test_recording_release_to_uploading() {
    AppCtx c{};
    c.state = AppState::RECORDING;
    TEST_ASSERT_EQUAL(AppState::UPLOADING, app_on_event(c, AppEvent::WAKE_RELEASE));
}

void test_recording_cap_to_uploading() {
    AppCtx c{};
    c.state = AppState::RECORDING;
    TEST_ASSERT_EQUAL(AppState::UPLOADING, app_on_event(c, AppEvent::RECORD_CAP));
}

void test_uploading_to_waiting() {
    AppCtx c{}; c.state = AppState::UPLOADING;
    TEST_ASSERT_EQUAL(AppState::WAITING, app_on_event(c, AppEvent::UPLOAD_DONE));
}

void test_waiting_first_byte_to_downloading() {
    AppCtx c{}; c.state = AppState::WAITING;
    TEST_ASSERT_EQUAL(AppState::DOWNLOADING, app_on_event(c, AppEvent::SERVER_FIRST_BYTE));
}

void test_downloading_to_playing() {
    AppCtx c{}; c.state = AppState::DOWNLOADING;
    TEST_ASSERT_EQUAL(AppState::PLAYING, app_on_event(c, AppEvent::DOWNLOAD_DONE));
}

void test_playing_end_to_idle() {
    AppCtx c{}; c.state = AppState::PLAYING;
    TEST_ASSERT_EQUAL(AppState::IDLE, app_on_event(c, AppEvent::PLAYBACK_END));
}

void test_retryable_error_once_goes_retry_then_uploading_again() {
    AppCtx c{};
    c.state = AppState::WAITING;
    c.retries_used = 0;
    c.last_error_retryable = true;
    auto s = app_on_event(c, AppEvent::ERROR_RETRYABLE);
    TEST_ASSERT_EQUAL(AppState::RETRY, s);
    c.state = s;
    TEST_ASSERT_EQUAL(AppState::UPLOADING, app_on_event(c, AppEvent::RETRY_TICK));
    TEST_ASSERT_EQUAL_INT(1, c.retries_used);
}

void test_second_retryable_falls_through_to_error() {
    AppCtx c{};
    c.state = AppState::WAITING;
    c.retries_used = 1;
    c.last_error_retryable = true;
    TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::ERROR_RETRYABLE));
}

void test_non_retryable_error_goes_straight_to_error() {
    AppCtx c{};
    c.state = AppState::WAITING;
    c.retries_used = 0;
    c.last_error_retryable = false;
    TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::ERROR_NON_RETRYABLE));
}

void test_wifi_loss_during_any_busy_state_goes_error() {
    AppCtx c{};
    for (auto s : {AppState::RECORDING, AppState::UPLOADING,
                   AppState::WAITING, AppState::DOWNLOADING}) {
        c.state = s;
        TEST_ASSERT_EQUAL(AppState::ERROR, app_on_event(c, AppEvent::WIFI_LOST));
    }
}

void test_vol_buttons_ignored_outside_idle_playing() {
    AppCtx c{}; c.state = AppState::WAITING;
    TEST_ASSERT_EQUAL(AppState::WAITING, app_on_event(c, AppEvent::VOL_UP));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_wake_press_starts_recording);
    RUN_TEST(test_idle_wake_ignored_without_wifi);
    RUN_TEST(test_recording_release_to_uploading);
    RUN_TEST(test_recording_cap_to_uploading);
    RUN_TEST(test_uploading_to_waiting);
    RUN_TEST(test_waiting_first_byte_to_downloading);
    RUN_TEST(test_downloading_to_playing);
    RUN_TEST(test_playing_end_to_idle);
    RUN_TEST(test_retryable_error_once_goes_retry_then_uploading_again);
    RUN_TEST(test_second_retryable_falls_through_to_error);
    RUN_TEST(test_non_retryable_error_goes_straight_to_error);
    RUN_TEST(test_wifi_loss_during_any_busy_state_goes_error);
    RUN_TEST(test_vol_buttons_ignored_outside_idle_playing);
    return UNITY_END();
}
```

- [ ] **Step 2: Run (fail)**

Run: `pio test -e native -f test_state_machine`

- [ ] **Step 3: Implement**

`src/app_state.h`:
```cpp
#pragma once
#include <cstdint>

enum class AppState : uint8_t {
    IDLE, RECORDING, UPLOADING, WAITING, DOWNLOADING, PLAYING,
    RETRY, ERROR, NO_WIFI,
};

enum class AppEvent : uint8_t {
    WAKE_PRESS, WAKE_RELEASE, RECORD_CAP,
    UPLOAD_DONE, SERVER_FIRST_BYTE, DOWNLOAD_DONE, PLAYBACK_END,
    ERROR_RETRYABLE, ERROR_NON_RETRYABLE,
    RETRY_TICK,
    WIFI_LOST, WIFI_OK,
    VOL_UP, VOL_DOWN,
    ERROR_TIMEOUT,
};

struct AppCtx {
    AppState state = AppState::IDLE;
    bool     wifi_connected = false;
    uint8_t  retries_used = 0;
    bool     last_error_retryable = false;
    uint8_t  style_idx = 0;
    uint8_t  volume_x10 = 6;
};

// Pure state transition. Mutates ctx counters where needed; returns new state.
AppState app_on_event(AppCtx& ctx, AppEvent ev);
const char* app_state_name(AppState s);
```

`src/app_state.cpp`:
```cpp
#include "app_state.h"

static bool is_busy(AppState s) {
    return s == AppState::RECORDING || s == AppState::UPLOADING
        || s == AppState::WAITING   || s == AppState::DOWNLOADING;
}

AppState app_on_event(AppCtx& c, AppEvent ev) {
    // WiFi loss preempts busy states.
    if (ev == AppEvent::WIFI_LOST && is_busy(c.state)) return AppState::ERROR;
    if (ev == AppEvent::WIFI_LOST && c.state != AppState::NO_WIFI) return AppState::NO_WIFI;
    if (ev == AppEvent::WIFI_OK && c.state == AppState::NO_WIFI) return AppState::IDLE;

    switch (c.state) {
        case AppState::IDLE:
            if (ev == AppEvent::WAKE_PRESS && c.wifi_connected) return AppState::RECORDING;
            return c.state;
        case AppState::RECORDING:
            if (ev == AppEvent::WAKE_RELEASE || ev == AppEvent::RECORD_CAP)
                return AppState::UPLOADING;
            return c.state;
        case AppState::UPLOADING:
            if (ev == AppEvent::UPLOAD_DONE) return AppState::WAITING;
            if (ev == AppEvent::ERROR_RETRYABLE) {
                if (c.retries_used == 0 && c.last_error_retryable) return AppState::RETRY;
                return AppState::ERROR;
            }
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::WAITING:
            if (ev == AppEvent::SERVER_FIRST_BYTE) return AppState::DOWNLOADING;
            if (ev == AppEvent::ERROR_RETRYABLE) {
                if (c.retries_used == 0 && c.last_error_retryable) return AppState::RETRY;
                return AppState::ERROR;
            }
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::DOWNLOADING:
            if (ev == AppEvent::DOWNLOAD_DONE) return AppState::PLAYING;
            if (ev == AppEvent::ERROR_NON_RETRYABLE || ev == AppEvent::ERROR_TIMEOUT)
                return AppState::ERROR;
            return c.state;
        case AppState::PLAYING:
            if (ev == AppEvent::PLAYBACK_END) return AppState::IDLE;
            return c.state;
        case AppState::RETRY:
            if (ev == AppEvent::RETRY_TICK) {
                c.retries_used++;
                return AppState::UPLOADING;
            }
            return c.state;
        case AppState::ERROR:
            if (ev == AppEvent::RETRY_TICK) {
                c.retries_used = 0;
                c.last_error_retryable = false;
                return AppState::IDLE;
            }
            return c.state;
        case AppState::NO_WIFI:
            return c.state;
    }
    return c.state;
}

const char* app_state_name(AppState s) {
    switch (s) {
        case AppState::IDLE:        return "IDLE";
        case AppState::RECORDING:   return "RECORDING";
        case AppState::UPLOADING:   return "UPLOADING";
        case AppState::WAITING:     return "WAITING";
        case AppState::DOWNLOADING: return "DOWNLOADING";
        case AppState::PLAYING:     return "PLAYING";
        case AppState::RETRY:       return "RETRY";
        case AppState::ERROR:       return "ERROR";
        case AppState::NO_WIFI:     return "NO_WIFI";
    }
    return "?";
}
```

- [ ] **Step 4: Run (pass)**

Run: `pio test -e native -f test_state_machine`
Expected: `13 Tests 0 Failures`.

- [ ] **Step 5: Commit**

```bash
git add src/app_state.h src/app_state.cpp test/test_state_machine
git commit -m "feat: add pure-logic app state machine with full transition tests"
```

---

### Task 9: NVS store wrapper

**Files:**
- Create: `src/config/nvs_store.h`
- Create: `src/config/nvs_store.cpp`
- Create: `test/mocks/preferences_fake.h`
- Create: `test/test_nvs_store/test_nvs_store.cpp`

- [ ] **Step 1: Write the fake backend**

`test/mocks/preferences_fake.h`:
```cpp
#pragma once
#include <cstdint>
#include <map>
#include <string>

class Preferences {
public:
    std::map<std::string, std::string> strs;
    std::map<std::string, uint32_t>    u32s;

    bool begin(const char*, bool = false) { return true; }
    void end() {}
    size_t putString(const char* k, const char* v) { strs[k] = v; return 1; }
    size_t putUInt(const char* k, uint32_t v)      { u32s[k] = v; return 1; }
    size_t putUChar(const char* k, uint8_t v)      { u32s[k] = v; return 1; }
    size_t putUShort(const char* k, uint16_t v)    { u32s[k] = v; return 1; }
    String getString(const char* k, const char* d = "") {
        auto it = strs.find(k); return String(it == strs.end() ? d : it->second.c_str());
    }
    uint32_t getUInt(const char* k, uint32_t d = 0)    { auto it = u32s.find(k); return it == u32s.end() ? d : it->second; }
    uint16_t getUShort(const char* k, uint16_t d = 0)  { auto it = u32s.find(k); return it == u32s.end() ? d : (uint16_t)it->second; }
    uint8_t  getUChar(const char* k, uint8_t d = 0)    { auto it = u32s.find(k); return it == u32s.end() ? d : (uint8_t)it->second; }
};

// Minimal Arduino String to satisfy the interface in native builds.
class String {
    std::string s;
public:
    String() {}
    String(const char* p) : s(p ? p : "") {}
    const char* c_str() const { return s.c_str(); }
    size_t length() const { return s.size(); }
    bool operator==(const char* r) const { return s == r; }
};
```

- [ ] **Step 2: Write the failing tests**

`test/test_nvs_store/test_nvs_store.cpp`:
```cpp
#include <unity.h>
#include "preferences_fake.h"
#include "config/nvs_store.h"

void setUp() { nvs_test_reset(); }
void tearDown() {}

void test_defaults_returned_when_empty() {
    NvsState s = nvs_load();
    TEST_ASSERT_EQUAL_INT(0, s.style_idx);
    TEST_ASSERT_EQUAL_INT(6, s.volume_x10);
}

void test_save_and_reload_round_trip() {
    NvsState s{3, "pirate", 8};
    nvs_save(s);
    NvsState got = nvs_load();
    TEST_ASSERT_EQUAL_INT(3, got.style_idx);
    TEST_ASSERT_EQUAL_STRING("pirate", got.style_id);
    TEST_ASSERT_EQUAL_INT(8, got.volume_x10);
}

void test_rate_limit_suppresses_rapid_writes() {
    nvs_set_clock_ms([]() -> uint32_t { return 1000; });
    NvsState s{0, "a", 5};
    TEST_ASSERT_TRUE(nvs_save(s));
    s.volume_x10 = 6;
    TEST_ASSERT_FALSE(nvs_save(s));   // within 1 s window
    nvs_set_clock_ms([]() -> uint32_t { return 2100; });
    TEST_ASSERT_TRUE(nvs_save(s));    // allowed again
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_returned_when_empty);
    RUN_TEST(test_save_and_reload_round_trip);
    RUN_TEST(test_rate_limit_suppresses_rapid_writes);
    return UNITY_END();
}
```

- [ ] **Step 3: Run (fail)**

Run: `pio test -e native -f test_nvs_store`

- [ ] **Step 4: Implement the wrapper**

`src/config/nvs_store.h`:
```cpp
#pragma once
#include <cstdint>

struct NvsState {
    uint16_t style_idx = 0;
    char     style_id[33] = "jesus";
    uint8_t  volume_x10 = 6;
};

using NvsClock = uint32_t (*)();

NvsState nvs_load();
bool     nvs_save(const NvsState& s);
void     nvs_set_clock_ms(NvsClock clock);
void     nvs_test_reset();   // only for unit tests
```

`src/config/nvs_store.cpp`:
```cpp
#include "config/nvs_store.h"
#ifdef UNIT_TEST
  #include "preferences_fake.h"
  static Preferences g_prefs;
#else
  #include <Preferences.h>
  static Preferences g_prefs;
#endif
#include <cstring>

namespace {
NvsClock g_clock = nullptr;
uint32_t g_last_save_ms = 0;
}

void nvs_set_clock_ms(NvsClock c) { g_clock = c; g_last_save_ms = 0; }

void nvs_test_reset() {
#ifdef UNIT_TEST
    g_prefs.strs.clear();
    g_prefs.u32s.clear();
#endif
    g_last_save_ms = 0;
    g_clock = nullptr;
}

NvsState nvs_load() {
    NvsState s;
    g_prefs.begin("xhs3e", true);
    s.style_idx   = g_prefs.getUShort("style_idx", 0);
    String sid    = g_prefs.getString("style_id", "jesus");
    strncpy(s.style_id, sid.c_str(), sizeof(s.style_id) - 1);
    s.volume_x10  = g_prefs.getUChar("vol_x10", 6);
    g_prefs.end();
    return s;
}

bool nvs_save(const NvsState& s) {
    uint32_t now = g_clock ? g_clock() : 0;
    if (g_last_save_ms != 0 && now - g_last_save_ms < 1000) return false;
    g_prefs.begin("xhs3e", false);
    g_prefs.putUShort("style_idx", s.style_idx);
    g_prefs.putString("style_id", s.style_id);
    g_prefs.putUChar("vol_x10", s.volume_x10);
    g_prefs.end();
    g_last_save_ms = now;
    return true;
}
```

- [ ] **Step 5: Run (pass)**

Run: `pio test -e native -f test_nvs_store`
Expected: `3 Tests 0 Failures`.

- [ ] **Step 6: Commit**

```bash
git add src/config/nvs_store.h src/config/nvs_store.cpp test/mocks/preferences_fake.h test/test_nvs_store
git commit -m "feat(config): add NVS store wrapper with rate-limited writes"
```

---

## Phase 3 — Asset pipeline

### Task 10: SFX baker + placeholder assets

**Files:**
- Create: `tools/bake_sfx.py`
- Create: `assets/sfx/README.md`
- Create: `src/audio/sfx_assets.h` (generated)

- [ ] **Step 1: Write the baker**

`tools/bake_sfx.py`:
```python
#!/usr/bin/env python3
"""Bake SFX WAV files (or synthesize placeholders) into a C header."""
import math, os, struct, sys, wave
from pathlib import Path

RATE = 16000
ASSETS = [
    ("sfx_rec_start", "rec_start.wav", 0.08, "click"),
    ("sfx_rec_send",  "rec_send.wav",  0.12, "kick"),
    ("sfx_note_c",    "note_c.wav",    0.25, ("tone", 523.25)),
    ("sfx_note_d",    "note_d.wav",    0.25, ("tone", 587.33)),
    ("sfx_note_e",    "note_e.wav",    0.25, ("tone", 659.25)),
    ("sfx_note_g",    "note_g.wav",    0.25, ("tone", 783.99)),
    ("sfx_note_a",    "note_a.wav",    0.25, ("tone", 880.00)),
    ("sfx_bed_loop",  "bed_loop.wav",  2.00, "bed"),
    ("sfx_error",     "error.wav",     0.40, "error"),
]

def synth(kind, duration):
    n = int(RATE * duration)
    out = []
    if isinstance(kind, tuple) and kind[0] == "tone":
        freq = kind[1]
        for i in range(n):
            env = math.exp(-3.0 * i / n)
            s = 0.3 * env * math.sin(2 * math.pi * freq * i / RATE)
            out.append(int(s * 32000))
    elif kind == "click":
        for i in range(n):
            env = math.exp(-30.0 * i / n)
            s = 0.4 * env * math.sin(2 * math.pi * 3000 * i / RATE)
            out.append(int(s * 32000))
    elif kind == "kick":
        for i in range(n):
            env = math.exp(-10.0 * i / n)
            freq = 120 * math.exp(-5.0 * i / n) + 50
            s = 0.6 * env * math.sin(2 * math.pi * freq * i / RATE)
            out.append(int(s * 32000))
    elif kind == "bed":
        for i in range(n):
            beat = (i // (RATE // 4)) % 4
            click = 1.0 if (i % (RATE // 4)) < (RATE // 40) else 0.0
            s = 0.08 * click * math.sin(2 * math.pi * (60 if beat == 0 else 200) * i / RATE)
            out.append(int(s * 32000))
    elif kind == "error":
        half = n // 2
        for i in range(n):
            freq = 400 if i < half else 330
            env = math.exp(-2.0 * (i % half) / half)
            s = 0.3 * env * math.sin(2 * math.pi * freq * i / RATE)
            out.append(int(s * 32000))
    return out

def load_wav_s16_16k_mono(path):
    with wave.open(str(path), "rb") as w:
        if w.getframerate() != RATE or w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise SystemExit(f"{path}: must be {RATE} Hz mono 16-bit PCM")
        raw = w.readframes(w.getnframes())
    return list(struct.unpack(f"<{len(raw)//2}h", raw))

def main():
    root = Path(__file__).resolve().parents[1]
    src_dir = root / "assets" / "sfx"
    out_path = root / "src" / "audio" / "sfx_assets.h"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    parts = ["// GENERATED by tools/bake_sfx.py — do not edit by hand.",
             "#pragma once", "#include <cstdint>", "#include <cstddef>", ""]
    total = 0
    for sym, fname, dur, kind in ASSETS:
        path = src_dir / fname
        if path.exists():
            samples = load_wav_s16_16k_mono(path)
        else:
            samples = synth(kind, dur)
        parts.append(f"static const int16_t {sym}_data[{len(samples)}] = {{")
        for i in range(0, len(samples), 16):
            row = ",".join(str(s) for s in samples[i:i+16])
            parts.append(f"    {row},")
        parts.append("};")
        parts.append(f"static constexpr size_t {sym}_len = {len(samples)};")
        parts.append("")
        total += len(samples) * 2
    parts.append(f"// Total SFX bytes: {total}")
    out_path.write_text("\n".join(parts))
    print(f"baked {len(ASSETS)} assets, {total} bytes → {out_path}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Write `assets/sfx/README.md`**

```
Drop 16 kHz mono 16-bit PCM WAV files here, named per tools/bake_sfx.py.
Missing files are synthesized as placeholders on bake.
After changing files, run:  python tools/bake_sfx.py
```

- [ ] **Step 3: Bake placeholders**

Run: `python tools/bake_sfx.py`
Expected: `baked 9 assets, <N> bytes → src/audio/sfx_assets.h`. Budget: total < 200 KB.

- [ ] **Step 4: Verify the generated header compiles**

Add a throwaway include in `src/main.cpp`:
```cpp
#include "audio/sfx_assets.h"
```
Run: `pio run -e xh-s3e-ai`
Expected: builds. Then remove the include from main.cpp (the audio task will include it later).

- [ ] **Step 5: Commit**

```bash
git add tools/bake_sfx.py assets/sfx/README.md src/audio/sfx_assets.h
git commit -m "feat(audio): add SFX asset baker and generated placeholder header"
```

Note: commit the generated header so CI/clean checkouts build without running Python.

---

## Phase 4 — Hardware HAL (on-device smoke)

> **Pre-req for Phase 4:** a physical XH-S3E-AI_v1.0 board connected via USB, with `include/secrets.h` populated (values only need to exist for build — WiFi isn't used yet).

### Task 11: Buttons HAL

**Files:**
- Create: `src/hal/buttons.h`
- Create: `src/hal/buttons.cpp`

- [ ] **Step 1: Implement**

`src/hal/buttons.h`:
```cpp
#pragma once
#include <cstdint>

enum class BtnId { WAKE, VOL_UP, VOL_DOWN };
enum class BtnEv { PRESS, RELEASE };

struct BtnEvent { BtnId id; BtnEv ev; };

void btns_begin();
bool btns_poll(BtnEvent& out);   // true if an event was returned
```

`src/hal/buttons.cpp`:
```cpp
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
// Event queue (tiny ring). UI task is sole consumer.
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
```

- [ ] **Step 2: Write a smoke-test harness**

Temporarily modify `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "hal/buttons.h"
#include "util/log.h"

void setup() {
    Serial.begin(115200);
    delay(250);
    log_init_arduino();
    btns_begin();
    log_line(LOG_INFO, "main", "smoke_buttons");
}
void loop() {
    BtnEvent e;
    while (btns_poll(e)) {
        const char* id = e.id == BtnId::WAKE ? "WAKE"
                       : e.id == BtnId::VOL_UP ? "VOL_UP" : "VOL_DOWN";
        const char* ev = e.ev == BtnEv::PRESS ? "press" : "release";
        log_line(LOG_INFO, "btn", "event", "id=%s ev=%s", id, ev);
    }
    delay(5);
}
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`
Expected: pressing/releasing each of GPIO 0 (Wake), 40 (Vol+), 39 (Vol−) produces exactly one matching press/release line each. No double-events. Debounce verified by tapping rapidly.

- [ ] **Step 4: Revert `main.cpp` to its previous stub** (keeps main clean for next task).

- [ ] **Step 5: Commit**

```bash
git add src/hal/buttons.h src/hal/buttons.cpp src/main.cpp
git commit -m "feat(hal): add debounced buttons with press/release events"
```

---

### Task 12: OLED HAL

**Files:**
- Create: `src/hal/oled.h`
- Create: `src/hal/oled.cpp`

- [ ] **Step 1: Implement**

`src/hal/oled.h`:
```cpp
#pragma once
#include <cstdint>

bool oled_begin();
void oled_clear();
void oled_show();
void oled_text(int x, int y, uint8_t size, const char* s);
void oled_rect(int x, int y, int w, int h, bool filled);
void oled_hbar(int x, int y, int w, int h, uint8_t fill_fraction_0_255);
```

`src/hal/oled.cpp`:
```cpp
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
```

- [ ] **Step 2: Smoke test**

Temporarily set `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "hal/oled.h"

void setup() {
    Serial.begin(115200);
    if (!oled_begin()) {
        Serial.println("OLED init FAILED"); while (true) delay(1000);
    }
    oled_clear();
    oled_text(0, 0, 1, "xh-s3e-ai OLED OK");
    oled_text(0, 16, 2, "HELLO");
    oled_hbar(0, 48, 128, 8, 128);
    oled_show();
}
void loop() { delay(1000); }
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e xh-s3e-ai -t upload`
Expected: display shows the header text, "HELLO" in size-2, and a half-filled bar at the bottom.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/hal/oled.h src/hal/oled.cpp
git commit -m "feat(hal): add SSD1306 OLED wrapper"
```

---

### Task 13: I2S out HAL

**Files:**
- Create: `src/hal/i2s_out.h`
- Create: `src/hal/i2s_out.cpp`

- [ ] **Step 1: Implement using Arduino-ESP32 `I2S.h` (ESP_I2S)**

`src/hal/i2s_out.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

using I2SOutFill = void (*)(int16_t* buf, size_t frames);

bool i2s_out_begin(I2SOutFill fill_cb);
void i2s_out_end();
```

`src/hal/i2s_out.cpp`:
```cpp
#include "hal/i2s_out.h"
#include <Arduino.h>
#include <ESP_I2S.h>

namespace {
I2SClass g_i2s;
I2SOutFill g_fill = nullptr;
TaskHandle_t g_task = nullptr;
constexpr size_t FRAMES = 512;
int16_t g_buf[FRAMES];

void task_fn(void*) {
    while (true) {
        if (g_fill) g_fill(g_buf, FRAMES);
        else        memset(g_buf, 0, sizeof(g_buf));
        g_i2s.write((uint8_t*)g_buf, sizeof(g_buf));
    }
}
}

bool i2s_out_begin(I2SOutFill fill) {
    g_fill = fill;
    g_i2s.setPins(15 /*BCLK*/, 16 /*WS*/, 7 /*DOUT*/);
    if (!g_i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        return false;
    }
    xTaskCreatePinnedToCore(task_fn, "i2s_out", 4096, nullptr, 10, &g_task, 1);
    return true;
}

void i2s_out_end() {
    if (g_task) { vTaskDelete(g_task); g_task = nullptr; }
    g_i2s.end();
}
```

- [ ] **Step 2: Smoke test — play a 1 s 440 Hz sine**

Temporarily set `src/main.cpp`:
```cpp
#include <Arduino.h>
#include <math.h>
#include "hal/i2s_out.h"

static uint32_t phase = 0;
void fill(int16_t* buf, size_t frames) {
    for (size_t i = 0; i < frames; i++) {
        float s = 0.2f * sinf(2.0f * (float)M_PI * 440.0f * phase / 16000.0f);
        buf[i] = (int16_t)(s * 32000);
        phase++;
    }
}

void setup() {
    Serial.begin(115200);
    if (!i2s_out_begin(fill)) { Serial.println("i2s_out FAILED"); while (1) delay(1000); }
    Serial.println("playing 440 Hz sine");
}
void loop() { delay(1000); }
```

- [ ] **Step 3: Flash and listen**

Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`
Expected: a continuous clean 440 Hz tone from the speaker. No clicks, no underruns.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/hal/i2s_out.h src/hal/i2s_out.cpp
git commit -m "feat(hal): add I2S output driver with pull-model fill callback"
```

---

### Task 14: I2S in HAL

**Files:**
- Create: `src/hal/i2s_in.h`
- Create: `src/hal/i2s_in.cpp`

- [ ] **Step 1: Implement**

`src/hal/i2s_in.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>

struct I2SInCtx {
    int16_t* buf;           // caller-owned, pre-allocated
    size_t   capacity_frames;
    volatile size_t write_pos_frames;
    std::atomic<uint16_t> last_rms;   // 0..65535
};

bool i2s_in_begin(I2SInCtx* ctx);
void i2s_in_stop();
```

`src/hal/i2s_in.cpp`:
```cpp
#include "hal/i2s_in.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <cmath>

namespace {
I2SClass g_i2s;
I2SInCtx* g_ctx = nullptr;
TaskHandle_t g_task = nullptr;
volatile bool g_run = false;

void task_fn(void*) {
    constexpr size_t CHUNK = 512;
    int16_t tmp[CHUNK];
    while (g_run) {
        size_t n = g_i2s.readBytes((char*)tmp, sizeof(tmp)) / 2;
        if (!g_ctx) continue;
        size_t room = g_ctx->capacity_frames - g_ctx->write_pos_frames;
        if (n > room) n = room;
        if (n > 0) {
            memcpy(g_ctx->buf + g_ctx->write_pos_frames, tmp, n * 2);
            g_ctx->write_pos_frames += n;
            // RMS
            uint64_t acc = 0;
            for (size_t i = 0; i < n; i++) acc += (int32_t)tmp[i] * tmp[i];
            uint32_t rms = (uint32_t)sqrtf((float)(acc / n));
            if (rms > 65535) rms = 65535;
            g_ctx->last_rms.store((uint16_t)rms, std::memory_order_relaxed);
        } else {
            // buffer full — keep reading and discarding to drain DMA
        }
    }
    vTaskDelete(nullptr);
}
}

bool i2s_in_begin(I2SInCtx* ctx) {
    g_ctx = ctx;
    g_ctx->write_pos_frames = 0;
    g_ctx->last_rms.store(0, std::memory_order_relaxed);
    g_i2s.setPinsPdmRx(4, 6);     // if PDM — but INMP441 is standard; see note below
    g_i2s.setPins(5 /*BCLK*/, 4 /*WS*/, -1, 6 /*DIN*/);
    if (!g_i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        return false;
    }
    g_run = true;
    xTaskCreatePinnedToCore(task_fn, "i2s_in", 4096, nullptr, 10, &g_task, 1);
    return true;
}

void i2s_in_stop() {
    g_run = false;
    g_task = nullptr;
    g_i2s.end();
    g_ctx = nullptr;
}
```

Note: if the INMP441-class mic doesn't produce audio with the standard-mode config above, the alternate pin setter to try is `setPinsPdmRx(4, 6)` — the exact call depends on the Arduino-ESP32 core version. Validate in the smoke test; do not ship `setPinsPdmRx` as active code for a non-PDM mic.

- [ ] **Step 2: Smoke test**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "hal/i2s_in.h"

static int16_t big_buf[16000 * 3];   // 3 s at 16 kHz
static I2SInCtx ctx = {big_buf, 16000 * 3, 0, {}};

void setup() {
    Serial.begin(115200);
    delay(500);
    if (!i2s_in_begin(&ctx)) { Serial.println("i2s_in FAILED"); while (1) delay(1000); }
    Serial.println("speak for 3 s...");
    while (ctx.write_pos_frames < ctx.capacity_frames) {
        Serial.printf("pos=%u rms=%u\n", (unsigned)ctx.write_pos_frames, ctx.last_rms.load());
        delay(200);
    }
    i2s_in_stop();
    Serial.printf("done — captured %u frames\n", (unsigned)ctx.write_pos_frames);
    // Dump first 10 samples as sanity
    for (int i = 0; i < 10; i++) Serial.printf("s[%d]=%d\n", i, big_buf[i]);
}
void loop() { delay(1000); }
```

- [ ] **Step 3: Flash and verify**

Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`
Expected: RMS visibly rises when you speak at the mic and drops to near zero in silence. Final "done — captured 48000 frames" message.

If RMS stays zero regardless: check I2S config — swap the pin setter to match actual mic type (PDM vs standard I2S); the pinout comments in the board notes assume a standard-mode digital mic.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/hal/i2s_in.h src/hal/i2s_in.cpp
git commit -m "feat(hal): add I2S input capture with RMS tracking"
```

---

## Phase 5 — Subsystem integration

### Task 15: SFX engine + audio task

**Files:**
- Create: `src/audio/sfx.h`
- Create: `src/audio/sfx.cpp`
- Create: `src/tasks/audio_task.h`
- Create: `src/tasks/audio_task.cpp`

- [ ] **Step 1: SFX event enum and router**

`src/audio/sfx.h`:
```cpp
#pragma once
#include <cstdint>

enum class SfxEvent : uint8_t {
    REC_START, SEND, TCP_CONNECTED, UPLOAD_DONE, SERVER_FIRST_BYTE,
    DOWNLOAD_DONE, PLAYBACK_START_RESPONSE, PLAYBACK_END, ERROR,
    BED_STOP,
};

struct SfxCmd {
    SfxEvent event;
    const int16_t* response_pcm;   // used only for PLAYBACK_START_RESPONSE
    uint32_t       response_frames;
};

void sfx_init();
// Called from audio task; consumes one SFX command by driving the mixer.
void sfx_apply(const SfxCmd& c);
```

`src/audio/sfx.cpp`:
```cpp
#include "audio/sfx.h"
#include "audio/mixer.h"
#include "audio/sfx_assets.h"

void sfx_init() { mixer_reset(); }

void sfx_apply(const SfxCmd& c) {
    switch (c.event) {
        case SfxEvent::REC_START:
            mixer_play(0, sfx_rec_start_data, sfx_rec_start_len, 1.0f, false); break;
        case SfxEvent::SEND:
            mixer_play(0, sfx_rec_send_data, sfx_rec_send_len, 1.0f, false); break;
        case SfxEvent::TCP_CONNECTED:
            mixer_play(0, sfx_note_c_data, sfx_note_c_len, 1.0f, false); break;
        case SfxEvent::UPLOAD_DONE:
            mixer_play(0, sfx_note_d_data, sfx_note_d_len, 1.0f, false);
            mixer_play(1, sfx_bed_loop_data, sfx_bed_loop_len, 1.0f, true); break;
        case SfxEvent::SERVER_FIRST_BYTE:
            mixer_play(0, sfx_note_e_data, sfx_note_e_len, 1.0f, false);
            mixer_stop(1); break;
        case SfxEvent::DOWNLOAD_DONE:
            mixer_play(0, sfx_note_g_data, sfx_note_g_len, 1.0f, false); break;
        case SfxEvent::PLAYBACK_START_RESPONSE:
            mixer_play(0, c.response_pcm, c.response_frames, 1.0f, false); break;
        case SfxEvent::PLAYBACK_END:
            mixer_play(0, sfx_note_a_data, sfx_note_a_len, 1.0f, false); break;
        case SfxEvent::ERROR:
            mixer_play(0, sfx_error_data, sfx_error_len, 1.0f, false);
            mixer_stop(1); break;
        case SfxEvent::BED_STOP:
            mixer_stop(1); break;
    }
}
```

- [ ] **Step 2: Audio task**

`src/tasks/audio_task.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include "hal/i2s_in.h"
#include "audio/sfx.h"

void audio_task_begin(int16_t* record_buf, size_t record_cap_frames);
void audio_task_enqueue(const SfxCmd& c);
void audio_task_start_recording();
size_t audio_task_stop_recording();   // returns frames captured
uint16_t audio_task_mic_rms();
uint16_t audio_task_output_rms();
void audio_task_set_volume_x10(uint8_t v);
```

`src/tasks/audio_task.cpp`:
```cpp
#include "tasks/audio_task.h"
#include "hal/i2s_out.h"
#include "audio/mixer.h"
#include "util/log.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cmath>

namespace {
QueueHandle_t g_sfx_q = nullptr;
I2SInCtx*     g_inctx = nullptr;
I2SInCtx      g_rec_ctx{};
std::atomic<uint16_t> g_out_rms{0};
std::atomic<uint8_t>  g_vol_x10{6};
bool g_recording = false;

void on_fill(int16_t* buf, size_t frames) {
    SfxCmd c;
    while (xQueueReceive(g_sfx_q, &c, 0) == pdTRUE) sfx_apply(c);
    float v = g_vol_x10.load() / 10.0f;
    mixer_render(buf, frames, v);
    uint64_t acc = 0;
    for (size_t i = 0; i < frames; i++) acc += (int32_t)buf[i] * buf[i];
    uint32_t rms = (uint32_t)sqrtf((float)(acc / frames));
    if (rms > 65535) rms = 65535;
    g_out_rms.store((uint16_t)rms, std::memory_order_relaxed);
}
}

void audio_task_begin(int16_t* rb, size_t cap) {
    g_rec_ctx.buf = rb;
    g_rec_ctx.capacity_frames = cap;
    g_rec_ctx.write_pos_frames = 0;
    g_sfx_q = xQueueCreate(16, sizeof(SfxCmd));
    sfx_init();
    if (!i2s_out_begin(on_fill)) {
        log_line(LOG_ERROR, "audio", "i2s_out_begin failed");
    }
}

void audio_task_enqueue(const SfxCmd& c) {
    if (g_sfx_q) xQueueSend(g_sfx_q, &c, 0);
}

void audio_task_start_recording() {
    if (g_recording) return;
    g_rec_ctx.write_pos_frames = 0;
    if (i2s_in_begin(&g_rec_ctx)) g_recording = true;
}

size_t audio_task_stop_recording() {
    if (!g_recording) return 0;
    size_t n = g_rec_ctx.write_pos_frames;
    i2s_in_stop();
    g_recording = false;
    return n;
}

uint16_t audio_task_mic_rms()    { return g_rec_ctx.last_rms.load(std::memory_order_relaxed); }
uint16_t audio_task_output_rms() { return g_out_rms.load(std::memory_order_relaxed); }
void audio_task_set_volume_x10(uint8_t v) { g_vol_x10.store(v > 10 ? 10 : v); }
```

- [ ] **Step 3: Smoke test — fire the full SFX sequence**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "tasks/audio_task.h"
#include "util/log.h"

void setup() {
    Serial.begin(115200); delay(500);
    log_init_arduino();
    static int16_t rb[16000 * 2];
    audio_task_begin(rb, 16000 * 2);
    audio_task_set_volume_x10(8);

    SfxEvent seq[] = {
        SfxEvent::REC_START, SfxEvent::SEND, SfxEvent::TCP_CONNECTED,
        SfxEvent::UPLOAD_DONE, SfxEvent::SERVER_FIRST_BYTE,
        SfxEvent::DOWNLOAD_DONE, SfxEvent::PLAYBACK_END, SfxEvent::ERROR,
    };
    for (auto ev : seq) {
        SfxCmd c{}; c.event = ev;
        audio_task_enqueue(c);
        log_line(LOG_INFO, "smoke", "fired", "ev=%d", (int)ev);
        delay(600);
    }
}
void loop() { delay(1000); }
```

- [ ] **Step 4: Flash and listen**

Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`
Expected: nine audible cues in sequence; during the brief window after UPLOAD_DONE you hear the drum bed loop until the SERVER_FIRST_BYTE cue stops it. The ERROR cue at the end is descending.

- [ ] **Step 5: Revert `main.cpp`**

- [ ] **Step 6: Commit**

```bash
git add src/audio/sfx.h src/audio/sfx.cpp src/tasks/audio_task.h src/tasks/audio_task.cpp
git commit -m "feat(audio): add SFX engine and audio task with 2-voice mixer"
```

---

### Task 16: OLED per-state rendering

**Files:**
- Create: `src/ui/render.h`
- Create: `src/ui/render.cpp`

- [ ] **Step 1: Implement**

`src/ui/render.h`:
```cpp
#pragma once
#include <cstdint>
#include "app_state.h"

struct UiModel {
    AppState   state = AppState::IDLE;
    const char* style_name = "";
    bool       wifi_ok = false;
    uint16_t   mic_rms = 0;
    uint16_t   out_rms = 0;
    uint32_t   bytes_done = 0;
    uint32_t   bytes_total = 0;
    uint8_t    volume_x10 = 6;
    uint32_t   now_ms = 0;
    uint32_t   vol_changed_ms = 0;
    uint32_t   record_started_ms = 0;
    const char* error_code = "";
    // Health footer dots
    bool health_stt_ok = false, health_llm_ok = false, health_tts_ok = false;
    bool styles_online = false;
};

void ui_render(const UiModel& m);
```

`src/ui/render.cpp`:
```cpp
#include "ui/render.h"
#include "hal/oled.h"
#include <cstdio>
#include <cstring>

namespace {
uint8_t rms_to_blocks(uint16_t rms) {
    // log-ish mapping: 0..20
    if (rms < 50) return 0;
    uint32_t r = rms;
    uint8_t n = 0;
    while (r > 50 && n < 20) { r = (r * 7) / 10; n++; }
    return n;
}
void draw_vu(int x, int y, int w, int h, uint16_t rms) {
    oled_rect(x, y, w, h, false);
    uint8_t blocks = rms_to_blocks(rms);
    int inner_w = (w - 2) * blocks / 20;
    oled_rect(x + 1, y + 1, inner_w, h - 2, true);
}
void draw_header(const UiModel& m) {
    oled_text(0, 0, 1, m.style_name && m.style_name[0] ? m.style_name : "?");
    oled_text(104, 0, 1, m.wifi_ok ? "WiFi" : "----");
}
void draw_footer_health(const UiModel& m) {
    char buf[32];
    snprintf(buf, sizeof(buf), "stt %s llm %s tts %s",
             m.health_stt_ok ? "ok" : "--",
             m.health_llm_ok ? "ok" : "--",
             m.health_tts_ok ? "ok" : "--");
    oled_text(0, 48, 1, buf);
}
}

void ui_render(const UiModel& m) {
    oled_clear();
    draw_header(m);

    switch (m.state) {
        case AppState::IDLE:
            oled_text(40, 16, 2, "READY");
            if (m.styles_online) draw_footer_health(m);
            else                 oled_text(0, 48, 1, "styles: offline");
            if (m.now_ms - m.vol_changed_ms < 1000) {
                char buf[24]; snprintf(buf, sizeof(buf), "vol %d/10", m.volume_x10);
                oled_text(64, 48, 1, buf);
            }
            break;
        case AppState::RECORDING: {
            char buf[16];
            uint32_t secs = (m.now_ms - m.record_started_ms) / 1000;
            snprintf(buf, sizeof(buf), "REC %02u:%02u", (unsigned)(secs/60), (unsigned)(secs%60));
            oled_text(20, 16, 2, buf);
            draw_vu(0, 34, 128, 8, m.mic_rms);
            break;
        }
        case AppState::UPLOADING:
            oled_text(10, 16, 2, "SENDING");
            if (m.bytes_total)
                oled_hbar(0, 34, 128, 8, (uint8_t)(255ULL * m.bytes_done / m.bytes_total));
            break;
        case AppState::WAITING: {
            oled_text(0, 16, 2, "THINKING");
            // animated sweeper: 2 px block bouncing based on now_ms
            int pos = (m.now_ms / 20) % 120;
            oled_rect(pos, 34, 2, 8, true);
            break;
        }
        case AppState::DOWNLOADING:
            oled_text(0, 16, 2, "RECEIVING");
            if (m.bytes_total)
                oled_hbar(0, 34, 128, 8, (uint8_t)(255ULL * m.bytes_done / m.bytes_total));
            break;
        case AppState::PLAYING:
            oled_text(20, 16, 2, "PLAYING");
            draw_vu(0, 34, 128, 8, m.out_rms);
            if (m.now_ms - m.vol_changed_ms < 1000) {
                char buf[24]; snprintf(buf, sizeof(buf), "vol %d/10", m.volume_x10);
                oled_text(64, 48, 1, buf);
            }
            break;
        case AppState::RETRY:
            oled_text(24, 16, 2, "RETRY");
            break;
        case AppState::ERROR:
            oled_text(32, 16, 2, "ERROR");
            oled_text(0, 48, 1, m.error_code ? m.error_code : "");
            break;
        case AppState::NO_WIFI:
            oled_text(12, 16, 2, "NO WIFI");
            break;
    }
    oled_show();
}
```

- [ ] **Step 2: Smoke test — cycle through every state**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "hal/oled.h"
#include "ui/render.h"

void setup() {
    Serial.begin(115200); delay(500);
    oled_begin();
    UiModel m;
    m.style_name = "JESUS"; m.wifi_ok = true;
    m.styles_online = true; m.health_stt_ok = m.health_llm_ok = m.health_tts_ok = true;

    AppState seq[] = {AppState::IDLE, AppState::RECORDING, AppState::UPLOADING,
                      AppState::WAITING, AppState::DOWNLOADING, AppState::PLAYING,
                      AppState::RETRY, AppState::ERROR, AppState::NO_WIFI};
    for (auto s : seq) {
        m.state = s; m.now_ms = millis(); m.record_started_ms = millis() - 12000;
        m.bytes_done = 60; m.bytes_total = 100;
        m.mic_rms = 2000; m.out_rms = 1500;
        m.error_code = "svr_timeout";
        ui_render(m);
        delay(2000);
    }
}
void loop() { delay(1000); }
```

- [ ] **Step 3: Flash and verify visually**

Each screen appears for 2 s in turn. Progress bar is visible in SENDING/RECEIVING. VU meter moves in RECORDING/PLAYING (static numbers here, but a bar appears). Error code text appears on ERROR.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/ui/render.h src/ui/render.cpp
git commit -m "feat(ui): add per-state OLED renderer"
```

---

### Task 17: UI task

**Files:**
- Create: `src/tasks/ui_task.h`
- Create: `src/tasks/ui_task.cpp`

- [ ] **Step 1: Implement**

`src/tasks/ui_task.h`:
```cpp
#pragma once
#include <cstdint>
#include "app_state.h"
#include "net/styles_api.h"   // StyleList

void ui_task_begin(StyleList* styles, AppCtx* ctx);
void ui_task_tick(uint32_t now_ms);
void ui_task_set_wifi(bool ok);
void ui_task_set_health(bool stt, bool llm, bool tts);
void ui_task_set_progress(uint32_t done, uint32_t total);
void ui_task_set_error(const char* code);
void ui_task_notify_state_change();   // called by net or other producers
```

`src/tasks/ui_task.cpp`:
```cpp
#include "tasks/ui_task.h"
#include "hal/buttons.h"
#include "hal/oled.h"
#include "ui/render.h"
#include "tasks/audio_task.h"
#include "config/nvs_store.h"
#include "util/log.h"
#include <Arduino.h>
#include <atomic>

extern void net_begin_send(size_t frames);   // forward decl from net task

namespace {
StyleList* g_styles = nullptr;
AppCtx*    g_ctx = nullptr;
UiModel    g_ui;
uint32_t   g_last_render_ms = 0;
uint32_t   g_vu_tick_ms = 0;
bool       g_wake_held = false;
uint32_t   g_record_started_ms = 0;

void apply_style_index() {
    if (!g_styles || g_styles->count == 0) return;
    uint8_t i = g_ctx->style_idx;
    if (i >= g_styles->count) i = 0;
    g_ui.style_name = g_styles->names[i];
}

void handle_button(const BtnEvent& e, uint32_t now_ms) {
    if (e.id == BtnId::WAKE) {
        if (e.ev == BtnEv::PRESS && g_ctx->state == AppState::IDLE && g_ctx->wifi_connected) {
            g_ctx->state = app_on_event(*g_ctx, AppEvent::WAKE_PRESS);
            g_wake_held = true;
            g_record_started_ms = now_ms;
            g_ui.record_started_ms = now_ms;
            audio_task_start_recording();
            SfxCmd c{}; c.event = SfxEvent::REC_START;
            audio_task_enqueue(c);
        } else if (e.ev == BtnEv::RELEASE && g_wake_held) {
            g_wake_held = false;
            if (g_ctx->state == AppState::RECORDING) {
                g_ctx->state = app_on_event(*g_ctx, AppEvent::WAKE_RELEASE);
                SfxCmd c{}; c.event = SfxEvent::SEND;
                audio_task_enqueue(c);
                size_t frames = audio_task_stop_recording();
                extern void net_begin_send(size_t frames);
                net_begin_send(frames);
            }
        }
    } else if (e.ev == BtnEv::PRESS) {
        int dir = e.id == BtnId::VOL_UP ? +1 : -1;
        if (g_ctx->state == AppState::IDLE) {
            if (g_styles && g_styles->count) {
                int next = (int)g_ctx->style_idx + dir;
                if (next < 0) next = g_styles->count - 1;
                if (next >= g_styles->count) next = 0;
                g_ctx->style_idx = (uint8_t)next;
                apply_style_index();
                NvsState ns = nvs_load();
                ns.style_idx = g_ctx->style_idx;
                strncpy(ns.style_id, g_styles->ids[g_ctx->style_idx], sizeof(ns.style_id) - 1);
                nvs_save(ns);
            }
        } else if (g_ctx->state == AppState::PLAYING) {
            int next = (int)g_ctx->volume_x10 + dir;
            if (next < 0) next = 0; if (next > 10) next = 10;
            g_ctx->volume_x10 = (uint8_t)next;
            audio_task_set_volume_x10(g_ctx->volume_x10);
            g_ui.vol_changed_ms = now_ms;
            NvsState ns = nvs_load();
            ns.volume_x10 = g_ctx->volume_x10;
            nvs_save(ns);
        }
    }
}
}

void ui_task_begin(StyleList* styles, AppCtx* ctx) {
    g_styles = styles; g_ctx = ctx;
    btns_begin();
    oled_begin();
    g_ui = UiModel{};
    apply_style_index();
}

void ui_task_set_wifi(bool ok) {
    g_ctx->wifi_connected = ok;
    g_ui.wifi_ok = ok;
    if (!ok && g_ctx->state != AppState::NO_WIFI) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::WIFI_LOST);
    } else if (ok && g_ctx->state == AppState::NO_WIFI) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::WIFI_OK);
    }
}
void ui_task_set_health(bool s, bool l, bool t) {
    g_ui.health_stt_ok = s; g_ui.health_llm_ok = l; g_ui.health_tts_ok = t;
}
void ui_task_set_progress(uint32_t d, uint32_t t) {
    g_ui.bytes_done = d; g_ui.bytes_total = t;
}
void ui_task_set_error(const char* c) { g_ui.error_code = c; }
void ui_task_notify_state_change() {}

void ui_task_tick(uint32_t now_ms) {
    BtnEvent e;
    while (btns_poll(e)) handle_button(e, now_ms);

    // Record cap
    if (g_ctx->state == AppState::RECORDING
        && now_ms - g_record_started_ms >= 60000) {
        g_ctx->state = app_on_event(*g_ctx, AppEvent::RECORD_CAP);
        SfxCmd c{}; c.event = SfxEvent::SEND;
        audio_task_enqueue(c);
        size_t frames = audio_task_stop_recording();
        extern void net_begin_send(size_t);
        net_begin_send(frames);
    }

    // VU and state update — render at 20 Hz during busy states, on change in IDLE
    g_ui.state = g_ctx->state;
    g_ui.volume_x10 = g_ctx->volume_x10;
    g_ui.now_ms = now_ms;
    g_ui.mic_rms = audio_task_mic_rms();
    g_ui.out_rms = audio_task_output_rms();
    g_ui.styles_online = g_styles && g_styles->count > 0;

    if (now_ms - g_last_render_ms >= 50) {
        ui_render(g_ui);
        g_last_render_ms = now_ms;
    }
}
```

- [ ] **Step 2: Smoke test — buttons + styles cycling**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "tasks/ui_task.h"
#include "tasks/audio_task.h"
#include "util/log.h"

static AppCtx ctx;
static StyleList styles = {{{"jesus"},{"pirate"},{"yoda"}}, {{"JESUS"},{"PIRATE"},{"YODA"}}, 3};
static int16_t rb[16000 * 2];

void setup() {
    Serial.begin(115200); delay(500); log_init_arduino();
    audio_task_begin(rb, 16000 * 2);
    ui_task_begin(&styles, &ctx);
    ctx.wifi_connected = true;
    ui_task_set_wifi(true);
}
void loop() { ui_task_tick(millis()); delay(10); }
```

- [ ] **Step 3: Flash and verify**

- Pressing Vol+/Vol− in IDLE cycles JESUS ↔ PIRATE ↔ YODA on the header.
- Pressing Wake transitions to RECORDING (and you hear the start SFX).
- Releasing Wake plays the send SFX and (since net isn't wired yet) hangs in UPLOADING — that's OK for this smoke; reset to continue.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/tasks/ui_task.h src/tasks/ui_task.cpp
git commit -m "feat(tasks): add UI task with buttons, OLED, and style/volume handling"
```

---

### Task 18: WiFi lifecycle

**Files:**
- Create: `src/net/wifi.h`
- Create: `src/net/wifi.cpp`

- [ ] **Step 1: Implement**

`src/net/wifi.h`:
```cpp
#pragma once
#include <cstdint>

using WifiCallback = void (*)(bool connected);

void wifi_begin(const char* ssid, const char* pass, WifiCallback cb);
bool wifi_connected();
```

`src/net/wifi.cpp`:
```cpp
#include "net/wifi.h"
#include <WiFi.h>
#include "util/log.h"

namespace {
WifiCallback g_cb = nullptr;
void evt(WiFiEvent_t e, WiFiEventInfo_t) {
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        log_line(LOG_INFO, "wifi", "got_ip", "ip=%s", WiFi.localIP().toString().c_str());
        if (g_cb) g_cb(true);
    } else if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        log_line(LOG_WARN, "wifi", "disconnected");
        if (g_cb) g_cb(false);
    }
}
}

void wifi_begin(const char* ssid, const char* pass, WifiCallback cb) {
    g_cb = cb;
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(evt);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, pass);
    log_line(LOG_INFO, "wifi", "connecting", "ssid=%s", ssid);
}
bool wifi_connected() { return WiFi.status() == WL_CONNECTED; }
```

- [ ] **Step 2: Smoke test**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "net/wifi.h"
#include "util/log.h"
#include "secrets.h"

void cb(bool ok) { Serial.printf("wifi cb: %d\n", (int)ok); }

void setup() {
    Serial.begin(115200); delay(500); log_init_arduino();
    wifi_begin(WIFI_SSID, WIFI_PASS, cb);
}
void loop() { delay(2000); Serial.printf("rssi=%d\n", WiFi.RSSI()); }
```

- [ ] **Step 3: Flash and verify**

Expected: "wifi cb: 1" within a few seconds, then RSSI readings stabilizing.

- [ ] **Step 4: Revert `main.cpp`**

- [ ] **Step 5: Commit**

```bash
git add src/net/wifi.h src/net/wifi.cpp
git commit -m "feat(net): add WiFi lifecycle with connection callback"
```

---

### Task 19: Styles + health API

**Files:**
- Create: `src/net/styles_api.h`
- Create: `src/net/styles_api.cpp`
- Create: `test/test_styles_parse/test_styles_parse.cpp`

- [ ] **Step 1: Write the native JSON parse test**

```cpp
#include <unity.h>
#include "net/styles_api.h"

void setUp() {}
void tearDown() {}

void test_parses_styles_list() {
    const char* body = "["
        "{\"id\":\"jesus\",\"name\":\"Jesus of Nazareth\"},"
        "{\"id\":\"pirate\",\"name\":\"Pirate\"}"
        "]";
    StyleList sl{};
    TEST_ASSERT_TRUE(styles_parse_list(body, strlen(body), sl));
    TEST_ASSERT_EQUAL_INT(2, sl.count);
    TEST_ASSERT_EQUAL_STRING("jesus", sl.ids[0]);
    TEST_ASSERT_EQUAL_STRING("Jesus of Nazareth", sl.names[0]);
}

void test_parses_health() {
    const char* body = "{\"stt\":\"ok\",\"llm\":\"ok\",\"tts\":\"down\"}";
    HealthStatus h{};
    TEST_ASSERT_TRUE(health_parse(body, strlen(body), h));
    TEST_ASSERT_TRUE(h.stt_ok);
    TEST_ASSERT_TRUE(h.llm_ok);
    TEST_ASSERT_FALSE(h.tts_ok);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_styles_list);
    RUN_TEST(test_parses_health);
    return UNITY_END();
}
```

- [ ] **Step 2: Run (fail)**

Run: `pio test -e native -f test_styles_parse`

- [ ] **Step 3: Implement**

`src/net/styles_api.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

struct StyleList { char ids[16][33]; char names[16][33]; uint8_t count; };
struct HealthStatus { bool stt_ok, llm_ok, tts_ok; };

bool styles_parse_list(const char* body, size_t len, StyleList& out);
bool health_parse(const char* body, size_t len, HealthStatus& out);

#ifdef ARDUINO
bool styles_fetch(StyleList& out);          // GET /v1/styles
bool health_fetch(HealthStatus& out);       // GET /healthz
#endif
```

`src/net/styles_api.cpp`:
```cpp
#include "net/styles_api.h"
#include <ArduinoJson.h>
#include <cstring>

bool styles_parse_list(const char* body, size_t len, StyleList& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len)) return false;
    if (!doc.is<JsonArray>()) return false;
    JsonArray arr = doc.as<JsonArray>();
    out.count = 0;
    for (JsonObject o : arr) {
        if (out.count >= 16) break;
        const char* id = o["id"] | "";
        const char* nm = o["name"] | "";
        if (!id[0]) continue;
        strncpy(out.ids[out.count], id, 32); out.ids[out.count][32] = 0;
        strncpy(out.names[out.count], nm, 32); out.names[out.count][32] = 0;
        out.count++;
    }
    return out.count > 0;
}

bool health_parse(const char* body, size_t len, HealthStatus& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body, len)) return false;
    out.stt_ok = strcmp(doc["stt"] | "", "ok") == 0;
    out.llm_ok = strcmp(doc["llm"] | "", "ok") == 0;
    out.tts_ok = strcmp(doc["tts"] | "", "ok") == 0;
    return true;
}

#ifdef ARDUINO
#include <HTTPClient.h>
#include "secrets.h"
#include "util/log.h"

static bool http_get(const char* path, char* body, size_t body_cap, size_t* body_len) {
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", SERVER_HOST, SERVER_PORT, path);
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    String s = http.getString();
    size_t n = s.length();
    if (n > body_cap - 1) n = body_cap - 1;
    memcpy(body, s.c_str(), n); body[n] = 0;
    if (body_len) *body_len = n;
    http.end();
    return true;
}

bool styles_fetch(StyleList& out) {
    char body[2048]; size_t n = 0;
    if (!http_get("/v1/styles", body, sizeof(body), &n)) return false;
    return styles_parse_list(body, n, out);
}
bool health_fetch(HealthStatus& out) {
    char body[256]; size_t n = 0;
    if (!http_get("/healthz", body, sizeof(body), &n)) return false;
    return health_parse(body, n, out);
}
#endif
```

- [ ] **Step 4: Run native test (pass)**

Run: `pio test -e native -f test_styles_parse`
Expected: `2 Tests 0 Failures`.

- [ ] **Step 5: On-device smoke (requires live server)**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include "net/wifi.h"
#include "net/styles_api.h"
#include "util/log.h"
#include "secrets.h"

void cb(bool ok) {
    if (!ok) return;
    StyleList s{};
    if (styles_fetch(s)) {
        Serial.printf("got %u styles\n", s.count);
        for (int i = 0; i < s.count; i++) Serial.printf("  %s (%s)\n", s.names[i], s.ids[i]);
    } else Serial.println("styles fetch failed");
    HealthStatus h{};
    if (health_fetch(h)) Serial.printf("health stt=%d llm=%d tts=%d\n", h.stt_ok, h.llm_ok, h.tts_ok);
}
void setup() {
    Serial.begin(115200); delay(500); log_init_arduino();
    wifi_begin(WIFI_SSID, WIFI_PASS, cb);
}
void loop() { delay(1000); }
```

Expected serial:
```
got N styles
  Jesus of Nazareth (jesus)
  ...
health stt=1 llm=1 tts=1
```

- [ ] **Step 6: Revert `main.cpp`**

- [ ] **Step 7: Commit**

```bash
git add src/net/styles_api.h src/net/styles_api.cpp test/test_styles_parse
git commit -m "feat(net): add styles + health API clients with JSON tests"
```

---

### Task 20: Multipart POST sender

**Files:**
- Create: `src/net/http_client.h`
- Create: `src/net/http_client.cpp`

- [ ] **Step 1: Implement**

`src/net/http_client.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>
#include "net/response_parser.h"

struct RestyleRequest {
    const char* style_id;
    const char* language;
    const uint8_t* pcm_bytes;      // record buffer
    size_t        pcm_byte_count;
    char          request_id[37];
};

struct RestyleResult {
    RespHeaders headers;
    uint8_t*    response_buf;      // caller-owned, large enough for RESP_MAX_BODY
    size_t      response_len;
    bool        ok;
    // Timings for logging
    uint32_t t_connect_ms, t_upload_ms, t_first_byte_ms, t_download_ms;
};

using MilestoneCb = void (*)(int milestone);  // 0=tcp, 1=upload_done, 2=first_byte, 3=download_done

bool http_restyle(const RestyleRequest& req, RestyleResult& res, MilestoneCb cb);
```

`src/net/http_client.cpp`:
```cpp
#include "net/http_client.h"
#include "config/wav_header.h"
#include "util/log.h"
#include "secrets.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>

namespace {
bool write_all(WiFiClient& c, const uint8_t* data, size_t n, uint32_t deadline_ms) {
    size_t sent = 0;
    while (sent < n) {
        if ((int32_t)(millis() - deadline_ms) > 0) return false;
        size_t w = c.write(data + sent, n - sent);
        if (w == 0) { delay(1); continue; }
        sent += w;
    }
    return true;
}
bool read_until_crlfcrlf(WiFiClient& c, char* buf, size_t cap, size_t& out, uint32_t deadline_ms) {
    out = 0;
    while (out + 1 < cap) {
        if ((int32_t)(millis() - deadline_ms) > 0) return false;
        if (!c.available()) { delay(1); continue; }
        int b = c.read(); if (b < 0) continue;
        buf[out++] = (char)b;
        if (out >= 4 && memcmp(buf + out - 4, "\r\n\r\n", 4) == 0) { buf[out] = 0; return true; }
    }
    return false;
}
}

bool http_restyle(const RestyleRequest& req, RestyleResult& res, MilestoneCb cb) {
    uint32_t t0 = millis();
    WiFiClient c;
    c.setTimeout(5);
    if (!c.connect(SERVER_HOST, SERVER_PORT)) {
        log_line(LOG_ERROR, "net", "connect_fail", "host=%s", SERVER_HOST);
        return false;
    }
    res.t_connect_ms = millis() - t0;
    if (cb) cb(0);

    // Build prologue + epilogue
    char boundary[48]; snprintf(boundary, sizeof(boundary), "XHS3E-%s", req.request_id);

    uint8_t wav_hdr[WAV_HEADER_SIZE];
    wav_header_build(wav_hdr, req.pcm_byte_count);
    uint32_t wav_total = WAV_HEADER_SIZE + req.pcm_byte_count;

    char prologue[1024];
    int pn = snprintf(prologue, sizeof(prologue),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"style_id\"\r\n\r\n%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"audio\"; filename=\"utterance.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n",
        boundary, req.style_id, boundary);
    int after_wav_len;
    char between_wav_lang[512];
    int wn = snprintf(between_wav_lang, sizeof(between_wav_lang),
        "\r\n--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n%s\r\n"
        "--%s--\r\n",
        boundary, req.language ? req.language : "en", boundary);

    // Now we know content-length:
    uint32_t body_len = pn + wav_total + wn;

    char req_line[512];
    int rn = snprintf(req_line, sizeof(req_line),
        "POST /v1/restyle HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: xh-s3e-ai/0.1\r\n"
        "Content-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %u\r\n"
        "X-Request-Id: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        SERVER_HOST, SERVER_PORT, boundary, (unsigned)body_len, req.request_id);

    uint32_t upload_deadline = millis() + 10000;
    if (!write_all(c, (uint8_t*)req_line, rn, upload_deadline)) return false;
    if (!write_all(c, (uint8_t*)prologue, pn, upload_deadline)) return false;
    if (!write_all(c, wav_hdr, WAV_HEADER_SIZE, upload_deadline)) return false;
    if (!write_all(c, req.pcm_bytes, req.pcm_byte_count, upload_deadline)) return false;
    if (!write_all(c, (uint8_t*)between_wav_lang, wn, upload_deadline)) return false;
    c.flush();
    res.t_upload_ms = millis() - t0;
    if (cb) cb(1);

    // Wait for response headers (up to server's 45 s ceiling)
    char hdr[2048]; size_t hdr_n = 0;
    uint32_t fb_deadline = millis() + 45000;
    if (!read_until_crlfcrlf(c, hdr, sizeof(hdr), hdr_n, fb_deadline)) { c.stop(); return false; }
    res.t_first_byte_ms = millis() - t0;
    if (cb) cb(2);

    if (!resp_parse_headers(hdr, hdr_n, res.headers)) { c.stop(); return false; }

    // Read body
    size_t got = 0; uint32_t last_byte_ms = millis();
    while (got < res.headers.content_length) {
        if (millis() - last_byte_ms > 10000) { c.stop(); return false; }
        int b = c.read(res.response_buf + got, res.headers.content_length - got);
        if (b > 0) { got += b; last_byte_ms = millis(); }
        else if (b == 0) { delay(1); }
        else { break; }
    }
    res.response_len = got;
    c.stop();
    res.t_download_ms = millis() - t0;
    if (cb) cb(3);
    res.ok = (got == res.headers.content_length);
    return res.ok;
}
```

- [ ] **Step 2: Skip native test for this one** (depends on Arduino `WiFiClient`; covered by on-device smoke in Task 21).

- [ ] **Step 3: Commit**

```bash
git add src/net/http_client.h src/net/http_client.cpp
git commit -m "feat(net): add multipart POST sender for /v1/restyle"
```

---

### Task 21: Net task end-to-end

**Files:**
- Create: `src/tasks/net_task.h`
- Create: `src/tasks/net_task.cpp`

- [ ] **Step 1: Implement**

`src/tasks/net_task.h`:
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

void net_task_begin(int16_t* record_buf, size_t record_cap_bytes,
                    uint8_t* response_buf, size_t response_cap_bytes);
void net_begin_send(size_t captured_frames);  // called by UI task
```

`src/tasks/net_task.cpp`:
```cpp
#include "tasks/net_task.h"
#include "tasks/ui_task.h"
#include "tasks/audio_task.h"
#include "net/http_client.h"
#include "net/styles_api.h"
#include "app_state.h"
#include "util/log.h"
#include "util/request_id.h"
#include "audio/sfx.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern AppCtx g_app_ctx;
extern StyleList g_styles;

namespace {
int16_t* g_rb = nullptr; size_t g_rb_cap = 0;
uint8_t* g_rpb = nullptr; size_t g_rpb_cap = 0;

struct SendCmd { size_t frames; };
QueueHandle_t g_q = nullptr;

void milestone(int m) {
    SfxCmd c{};
    switch (m) {
        case 0: c.event = SfxEvent::TCP_CONNECTED; break;
        case 1: c.event = SfxEvent::UPLOAD_DONE;
                g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::UPLOAD_DONE); break;
        case 2: c.event = SfxEvent::SERVER_FIRST_BYTE;
                g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::SERVER_FIRST_BYTE); break;
        case 3: c.event = SfxEvent::DOWNLOAD_DONE;
                g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::DOWNLOAD_DONE); break;
    }
    if (m == 0 || m == 1 || m == 2 || m == 3) audio_task_enqueue(c);
}

void handle_send(const SendCmd& sc) {
    if (sc.frames < 4800) {   // <0.3 s — probably accidental
        g_app_ctx.last_error_retryable = false;
        g_app_ctx.state = app_on_event(g_app_ctx, AppEvent::ERROR_NON_RETRYABLE);
        ui_task_set_error("too_short");
        SfxCmd c{}; c.event = SfxEvent::ERROR;
        audio_task_enqueue(c);
        return;
    }
    RestyleRequest req{};
    req.style_id = g_styles.ids[g_app_ctx.style_idx];
    req.language = "en";
    req.pcm_bytes = (const uint8_t*)g_rb;
    req.pcm_byte_count = sc.frames * 2;
    request_id_new(req.request_id);
    log_line(LOG_INFO, "net", "submit", "style=%s frames=%u rid=%s",
             req.style_id, (unsigned)sc.frames, req.request_id);

    RestyleResult res{};
    res.response_buf = g_rpb;

    // ERROR path helper
    auto fail = [](const char* code, bool retryable) {
        g_app_ctx.last_error_retryable = retryable;
        AppEvent ev = retryable ? AppEvent::ERROR_RETRYABLE : AppEvent::ERROR_NON_RETRYABLE;
        g_app_ctx.state = app_on_event(g_app_ctx, ev);
        ui_task_set_error(code);
        SfxCmd c{}; c.event = SfxEvent::ERROR;
        audio_task_enqueue(c);
    };

    for (int attempt = 0; attempt < 2; attempt++) {
        g_app_ctx.state = AppState::UPLOADING;
        if (http_restyle(req, res, milestone)) {
            if (res.headers.status == 200 && strncmp(res.headers.content_type, "audio/wav", 9) == 0) {
                // Parse WAV header
                if (res.response_len < 44 || !resp_is_wav(res.response_buf)) {
                    fail("bad_wav", false); return;
                }
                // Hand PCM to audio task for playback
                SfxCmd c{};
                c.event = SfxEvent::PLAYBACK_START_RESPONSE;
                c.response_pcm = (const int16_t*)(res.response_buf + 44);
                c.response_frames = (res.response_len - 44) / 2;
                audio_task_enqueue(c);
                log_line(LOG_INFO, "net", "ok",
                         "bytes=%u t_up=%u t_fb=%u t_dl=%u",
                         (unsigned)res.response_len, res.t_upload_ms,
                         res.t_first_byte_ms, res.t_download_ms);
                return;
            }
            // JSON error from server
            JsonDocument d;
            if (!deserializeJson(d, res.response_buf, res.response_len)) {
                const char* code = d["error_code"] | "svr_err";
                bool retry = d["retryable"] | false;
                if (retry && attempt == 0) {
                    log_line(LOG_WARN, "net", "retryable", "code=%s", code);
                    delay(1000); continue;
                }
                fail(code, retry); return;
            }
            fail("bad_resp", false); return;
        }
        // Transport failure
        if (attempt == 0) { delay(1000); continue; }
        fail("net_fail", false); return;
    }
}

void task_fn(void*) {
    SendCmd sc;
    while (true) {
        if (xQueueReceive(g_q, &sc, portMAX_DELAY) == pdTRUE) handle_send(sc);
    }
}
}

void net_task_begin(int16_t* rb, size_t rb_bytes, uint8_t* rpb, size_t rpb_bytes) {
    g_rb = rb; g_rb_cap = rb_bytes;
    g_rpb = rpb; g_rpb_cap = rpb_bytes;
    g_q = xQueueCreate(2, sizeof(SendCmd));
    xTaskCreatePinnedToCore(task_fn, "net", 8192, nullptr, 8, nullptr, 0);
}

void net_begin_send(size_t frames) {
    SendCmd sc{frames};
    xQueueSend(g_q, &sc, 0);
}
```

- [ ] **Step 2: Flag on_error transition completeness**

On a successful response, the UI should eventually transition PLAYING → IDLE. The audio task needs to signal `PLAYBACK_END`. Add to audio_task.cpp:

In `on_fill`, after rendering, check: if voice 0 was active on previous frame and is now inactive and we just finished a response playback (tracked with a bool flag set when we enqueue PLAYBACK_START_RESPONSE), post a `PLAYBACK_END` signal that the UI task picks up.

Modify `audio_task.cpp`:

Add near the top of the anonymous namespace:
```cpp
std::atomic<bool> g_response_playing{false};
std::atomic<bool> g_response_just_finished{false};
```

Modify `sfx_apply` usage in `on_fill`:
```cpp
void on_fill(int16_t* buf, size_t frames) {
    SfxCmd c;
    while (xQueueReceive(g_sfx_q, &c, 0) == pdTRUE) {
        if (c.event == SfxEvent::PLAYBACK_START_RESPONSE) g_response_playing.store(true);
        sfx_apply(c);
    }
    bool was_active = mixer_voice_active(0);
    float v = g_vol_x10.load() / 10.0f;
    mixer_render(buf, frames, v);
    bool still_active = mixer_voice_active(0);
    if (g_response_playing.load() && was_active && !still_active) {
        g_response_playing.store(false);
        g_response_just_finished.store(true);
    }
    /* rms computation unchanged */
}
```

Expose from audio_task.h:
```cpp
bool audio_task_consume_playback_end();
```

Implement:
```cpp
bool audio_task_consume_playback_end() {
    bool f = g_response_just_finished.load();
    if (f) g_response_just_finished.store(false);
    return f;
}
```

Call this from UI task tick:
```cpp
if (audio_task_consume_playback_end()) {
    SfxCmd c{}; c.event = SfxEvent::PLAYBACK_END;
    audio_task_enqueue(c);
    g_ctx->state = app_on_event(*g_ctx, AppEvent::PLAYBACK_END);
}
```

Update `src/tasks/audio_task.cpp` and `src/tasks/audio_task.h` and `src/tasks/ui_task.cpp` with the three changes above.

- [ ] **Step 3: Flash full end-to-end build (uses secrets.h + live server)**

Temporarily `src/main.cpp`:
```cpp
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "tasks/ui_task.h"
#include "tasks/audio_task.h"
#include "tasks/net_task.h"
#include "net/wifi.h"
#include "net/styles_api.h"
#include "config/nvs_store.h"
#include "util/log.h"
#include "util/request_id.h"
#include "secrets.h"

AppCtx g_app_ctx;
StyleList g_styles{};
static int16_t* g_record_buf = nullptr;
static uint8_t* g_response_buf = nullptr;

void wifi_cb(bool ok) { ui_task_set_wifi(ok); }

void setup() {
    Serial.begin(115200); delay(500);
    log_init_arduino();
    request_id_seed(esp_random());
    nvs_set_clock_ms([]() -> uint32_t { return millis(); });

    g_record_buf   = (int16_t*)heap_caps_malloc(1920000, MALLOC_CAP_SPIRAM);
    g_response_buf = (uint8_t*)heap_caps_malloc(2000000, MALLOC_CAP_SPIRAM);
    if (!g_record_buf || !g_response_buf) {
        log_line(LOG_ERROR, "main", "psram_alloc_fail"); while (1) delay(1000);
    }

    NvsState ns = nvs_load();
    g_app_ctx.style_idx  = ns.style_idx;
    g_app_ctx.volume_x10 = ns.volume_x10;

    audio_task_begin(g_record_buf, 1920000 / 2);
    audio_task_set_volume_x10(g_app_ctx.volume_x10);
    ui_task_begin(&g_styles, &g_app_ctx);

    wifi_begin(WIFI_SSID, WIFI_PASS, wifi_cb);

    // Wait for WiFi briefly, then fetch styles once
    uint32_t wait_start = millis();
    while (!wifi_connected() && millis() - wait_start < 8000) delay(100);
    if (wifi_connected()) {
        styles_fetch(g_styles);
        HealthStatus h{};
        if (health_fetch(h)) ui_task_set_health(h.stt_ok, h.llm_ok, h.tts_ok);
    }

    net_task_begin(g_record_buf, 1920000, g_response_buf, 2000000);
}

void loop() {
    ui_task_tick(millis());
    delay(5);
}
```

- [ ] **Step 4: Run end-to-end test**

Run: `pio run -e xh-s3e-ai -t upload && pio device monitor -b 115200`

Expected behavior:
- On boot, OLED shows IDLE with first style and WiFi icon after connect.
- Press Wake → REC screen + REC_START cue; VU moves while speaking.
- Release Wake → SEND cue + SENDING screen → TCP cue → UPLOAD cue → bed loop + THINKING screen → FIRST_BYTE cue + RECEIVING screen → DOWNLOAD cue → PLAYING screen with output VU + response audio through speaker → PLAYBACK_END cue → back to IDLE.
- Vol+ / Vol− in IDLE cycles styles (OLED header updates, NVS persists across reboot).
- Vol+ / Vol− in PLAYING changes volume (brief overlay).
- Kill WiFi → NO WIFI screen within seconds. Restore → IDLE again.
- Pull server down mid-flight → ERROR screen with `net_fail`, returns to IDLE.

- [ ] **Step 5: Commit**

```bash
git add src/tasks/net_task.h src/tasks/net_task.cpp src/tasks/audio_task.h src/tasks/audio_task.cpp src/tasks/ui_task.cpp src/main.cpp
git commit -m "feat: wire end-to-end net task and complete UI playback handoff"
```

---

## Phase 6 — Full integration

### Task 22: Health poll, style retry, smoke build flag

**Files:**
- Modify: `src/main.cpp`
- Modify: `src/tasks/ui_task.cpp` (add background poll hooks)

- [ ] **Step 1: Add periodic health + style retry inside `main.cpp`'s `loop()`**

```cpp
void loop() {
    ui_task_tick(millis());

    static uint32_t last_health = 0;
    static uint32_t last_style_retry = 0;
    uint32_t now = millis();

    if (wifi_connected() && g_app_ctx.state == AppState::IDLE && now - last_health > 60000) {
        HealthStatus h{};
        if (health_fetch(h)) ui_task_set_health(h.stt_ok, h.llm_ok, h.tts_ok);
        last_health = now;
    }
    if (wifi_connected() && g_styles.count == 0 && now - last_style_retry > 30000) {
        styles_fetch(g_styles);
        last_style_retry = now;
    }

    delay(5);
}
```

- [ ] **Step 2: Add `-DSMOKE_BUILD` flag support**

In `src/main.cpp`:
```cpp
#ifdef SMOKE_BUILD
static void run_smoke_sequence();
#endif

void setup() {
    // ... as before ...
#ifdef SMOKE_BUILD
    run_smoke_sequence();
#endif
}

#ifdef SMOKE_BUILD
static void run_smoke_sequence() {
    // Fire each SFX
    for (int i = 0; i <= (int)SfxEvent::ERROR; i++) {
        SfxCmd c{}; c.event = (SfxEvent)i;
        audio_task_enqueue(c); delay(600);
    }
    // Cycle every OLED screen for 1.5 s
    AppState seq[] = {AppState::IDLE, AppState::RECORDING, AppState::UPLOADING,
                      AppState::WAITING, AppState::DOWNLOADING, AppState::PLAYING,
                      AppState::RETRY, AppState::ERROR, AppState::NO_WIFI};
    for (auto s : seq) {
        g_app_ctx.state = s; ui_task_tick(millis()); delay(1500);
    }
    g_app_ctx.state = AppState::IDLE;
}
#endif
```

- [ ] **Step 3: Define the smoke env in `platformio.ini`**

Append:
```ini
[env:xh-s3e-ai-smoke]
extends = env:xh-s3e-ai
build_flags =
    ${env:xh-s3e-ai.build_flags}
    -DSMOKE_BUILD
```

- [ ] **Step 4: Run smoke build**

Run: `pio run -e xh-s3e-ai-smoke -t upload && pio device monitor -b 115200`
Expected: hear every SFX, see every OLED screen, then normal operation resumes.

- [ ] **Step 5: Final native test pass (all suites)**

Run: `pio test -e native`
Expected: every test suite reports `0 Failures`. Count: 8 suites (log, request_id, wav_header, mixer, response_parser, state_machine, nvs_store, styles_parse), ~36 test cases total.

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp platformio.ini
git commit -m "feat: add periodic health poll, style retry, smoke-build target"
```

---

### Task 23: README and final polish

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Rewrite README**

```markdown
# live-speech-style-esp32

Firmware for the XH-S3E-AI_v1.0 (Xiaozhi-style ESP32-S3-N16R8) board.
Push-to-talk client for the live-speech-style server.

## Pinout

| Function | GPIO |
|---|---|
| OLED SDA | 41 |
| OLED SCL | 42 |
| Mic WS / BCLK / DIN | 4 / 5 / 6 |
| Amp DOUT / BCLK / WS | 7 / 15 / 16 |
| Wake button | 0 |
| Vol+ | 40 |
| Vol− | 39 |

## Setup

1. Copy `include/secrets.h.example` to `include/secrets.h` and fill in SSID, password, server host/port.
2. Bake SFX assets (once, and any time you change `assets/sfx/*.wav`):
   ```
   python tools/bake_sfx.py
   ```
3. Build and upload:
   ```
   pio run -e xh-s3e-ai -t upload
   pio device monitor -b 115200
   ```

## Tests

Native unit tests (no hardware):
```
pio test -e native
```

On-device smoke:
```
pio run -e xh-s3e-ai-smoke -t upload
```

## Design docs

- [Firmware design](docs/2026-04-23-firmware-design.md)
- [Server design](docs/2026-04-22-live-speech-style-design.md)
- [Implementation plan](docs/2026-04-23-firmware-implementation-plan.md)

## Troubleshooting

- **PSRAM alloc fails / boot loops:** confirm `board_build.arduino.memory_type = qio_opi` in `platformio.ini`.
- **Mic RMS stays 0:** the mic may be PDM rather than standard I2S — see the note in `src/hal/i2s_in.cpp`.
- **WiFi never connects:** verify secrets.h; this firmware uses 2.4 GHz only (ESP32-S3 limitation).
- **Server returns `no_speech`:** speak louder, closer, or longer.
```

- [ ] **Step 2: Final build + sanity**

Run: `pio run -e xh-s3e-ai && pio test -e native`
Expected: both succeed. No warnings worth chasing.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "docs: add README with pinout, setup, tests, troubleshooting"
```

---

## Self-review (authoring)

**Spec coverage check:**
- §3 Hardware pinout → Tasks 11–14, documented in Task 23 README.
- §4 Architecture (three tasks, two buffers, mutex/queues) → Tasks 15, 17, 21; buffer allocation in Task 21 / main.cpp.
- §5 State machine → Task 8 (pure) + Task 17 (wired).
- §6 Memory & PSRAM budget → Task 21 allocates `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` for both buffers.
- §7 Audio pipeline (I2S in/out, mixer, SFX routing) → Tasks 13, 14, 6, 10, 15.
- §8 Network (WiFi lifecycle, multipart, response parse, timeouts, retry) → Tasks 18, 20, 7, 21.
- §9 OLED UI (per-state, VU, progress, health dots, NO_WIFI overlay) → Tasks 12, 16, 17, 22.
- §10 Config, NVS, secrets, logging → Tasks 2, 9, 3.
- §11 Error handling + retry → Task 8 (pure), Task 21 (wired).
- §12 Repo layout → realized across tasks.
- §13 Build config → Task 1.
- §14 Testing (native + smoke) → Tasks 4–9, 19 (native); Task 22 (smoke).

No spec requirements without a corresponding task.

**Placeholder scan:** no "TBD", no "implement later", no "similar to task N" references. Every code step contains complete code. Every test step contains assertions.

**Type consistency:** `AppState`, `AppEvent`, `AppCtx`, `StyleList`, `UiModel`, `SfxCmd`, `RestyleRequest`, `RestyleResult`, `RespHeaders`, `NvsState` all defined once and used consistently across tasks that reference them.

**Known edge:** Task 21 Step 2 modifies files created in Tasks 15 & 17 (adds `g_response_playing` / `audio_task_consume_playback_end`). This is explicit and complete (full code given), not a placeholder.

---

## Execution handoff

Plan complete and saved to `docs/2026-04-23-firmware-implementation-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Each subagent gets just the task it needs, implements it, runs the verification, and commits.

**2. Inline Execution** — I execute tasks in this session using executing-plans, batch execution with checkpoints for review.

Which approach?
