# Live Speech Style ESP32-S3 Firmware — Design

**Status:** Draft, pending user review
**Date:** 2026-04-23
**Target hardware:** XH-S3E-AI_v1.0 Mini (ESP32-S3-WROOM-1-N16R8, 16 MB flash, 8 MB PSRAM)
**Companion service:** [live-speech-style server](2026-04-22-live-speech-style-design.md)

## 1. Goal

Custom firmware for the XH-S3E-AI_v1.0 board that turns it into a push-to-talk
client for the live-speech-style server. The user holds a button, speaks, releases;
the device uploads the utterance, waits for the styled response WAV, and plays it
back through the on-board I2S amplifier. A 0.96" SSD1306 OLED shows state; two
extra buttons cycle styles and adjust playback volume. Short embedded sound
effects cue network events so the flow is audible as well as visible.

## 2. Scope and non-goals

**In scope:**
- Arduino framework via PlatformIO (simplicity first; IDF rewrite later if needed).
- Hold-to-talk recording with a 60 s cap.
- Synchronous multipart POST to `/v1/restyle`, full-body download, local playback.
- Style selection by cycling a list fetched at boot from `GET /v1/styles`.
- On-device 2-voice audio mixer for a looping bed + one-shot SFX + response playback.
- OLED status UI with VU meter, progress bar, and error text.
- Hardcoded WiFi credentials and server URL via `include/secrets.h`.
- NVS persistence of the selected style and playback volume.

**Out of scope for v1 (YAGNI, not objections):**
- TLS, authentication, cert pinning.
- OTA firmware updates.
- Captive-portal or BLE WiFi provisioning.
- mDNS server discovery.
- Streaming upload-while-recording or streaming playback-while-downloading.
- 3+ voice mixer with response/SFX overlap.
- Transcript scrolling on the OLED.
- Stock Xiaozhi firmware protocol compatibility (Opus + WebSocket + Xiaozhi schema).

## 3. Hardware target and pin map

ESP32-S3-WROOM-1-N16R8: 16 MB QIO flash, 8 MB OPI PSRAM. Dual-core Xtensa LX7
at 240 MHz. Arduino core requires `board_build.arduino.memory_type = qio_opi`
for PSRAM to initialize; without it, large allocations crash the heap.

| Peripheral | Function | GPIO |
|---|---|---|
| I2C (OLED) | SDA | 41 |
| I2C (OLED) | SCL | 42 |
| I2S mic | WS / LRCK | 4 |
| I2S mic | SCK / BCLK | 5 |
| I2S mic | DIN | 6 |
| I2S amp | DOUT | 7 |
| I2S amp | BCLK | 15 |
| I2S amp | WS / LRCK | 16 |
| Button | Wake / push-to-talk | 0 |
| Button | Volume + / style next | 40 |
| Button | Volume − / style prev | 39 |
| — | Reset | EN / RST |

Assumed peripheral chips (based on the `bread-compact-wifi` reference):
INMP441-class digital mic on the input bus, MAX98357A-class Class-D amp on
the output bus, SSD1306 OLED on I2C.

## 4. Architecture

One Arduino/PlatformIO firmware with three long-lived FreeRTOS tasks, one
shared mutex-protected `AppState`, and two pre-allocated PSRAM buffers whose
ownership hands off by pointer between tasks.

```
 ┌────────────────────┐    ┌──────────────────────┐    ┌──────────────────────┐
 │  UI task (core 0)  │    │  Net task (core 0)   │    │ Audio task (core 1)  │
 │  - button debounce │    │  - WiFi event loop   │    │  - I2S in  (DMA)     │
 │  - OLED render     │◀──▶│  - HTTPClient POST   │◀──▶│  - I2S out (DMA)     │
 │  - state machine   │    │  - body download     │    │  - 2-voice mixer     │
 └────────────────────┘    └──────────────────────┘    └──────────────────────┘
        │   ▲                        │                          │    ▲
        │   │     FreeRTOS queues / task notifications          │    │
        ▼   │                        ▼                          ▼    │
   ┌──────────────────────────────────────────────────────────────────┐
   │                        Shared state (mutex)                      │
   │   AppState enum, current style_id, volume, last error            │
   │   Record buffer (1.92 MB PSRAM), response buffer (≤2 MB PSRAM)   │
   │   Embedded SFX sample table (flash)                              │
   └──────────────────────────────────────────────────────────────────┘
```

**Tasks:**
1. **UI task** (core 0, moderate priority). Owns the state machine and the OLED.
   Polls buttons at 100 Hz. Re-renders the OLED on state change plus a 20 Hz
   VU meter tick during `RECORDING` and `PLAYING`.
2. **Net task** (core 0, moderate priority). Owns WiFi and HTTP. Blocks on a
   queue of "send-utterance" commands; drives one request/response cycle;
   emits SFX event codes to the audio task at each TCP milestone.
3. **Audio task** (core 1, high priority). Owns both I2S peripherals and the
   mixer. Pinned to core 1 so DMA callbacks aren't preempted by WiFi ISRs.
   Never blocks on UI or net.

**Cross-task sync:**
- One FreeRTOS queue per consumer: `ui_events`, `net_commands`, `audio_events`.
- One mutex around the `AppState` struct.
- The record buffer and response buffer are single-owner; pointers are passed
  across queue messages and the sender loses access after send. Ownership is
  held for the full utterance lifetime, not per state:
  - **Record buffer:** Audio task owns it during `RECORDING`. On `UPLOADING`
    entry the ptr is handed to the net task, which retains it through
    `UPLOADING`/`WAITING`/`DOWNLOADING` and through any retry cycle. Released
    back to the free pool on terminal transition to `IDLE`.
  - **Response buffer:** Net task owns it during `DOWNLOADING`. On `PLAYING`
    entry the ptr is handed to the audio task, which releases it on
    `PLAYBACK_END`.

## 5. State machine

Nine states. Only the UI task writes the state enum; other tasks read under the
mutex and react via queues.

```
         ┌─────────────────────────────────────────────────────────┐
         │                                                         │
         ▼                                                         │
    ┌─────────┐   Wake press          ┌───────────┐    60 s cap    │
    │  IDLE   │ ────────────────────▶ │ RECORDING │─────────────┐  │
    │         │                       │           │             │  │
    │ (shows  │   Vol+/Vol-           │  capture  │             │  │
    │  style) │ ─── cycle style ─────▶│  mic→buf  │             │  │
    └─────────┘                       └───────────┘             │  │
         ▲                                  │ Wake release       │  │
         │                                  ▼                    ▼  │
         │                            ┌───────────┐         ┌────────┐
         │                            │ UPLOADING │────────▶│ WAITING│
         │                            └───────────┘  done   └────────┘
         │                                  │                    │
         │        transport err             │                    │ first byte
         │ ◀──────────────────┐             │                    ▼
         │                    │             │             ┌───────────┐
         │                    │             │             │DOWNLOADING│
         │                    │             ▼             └───────────┘
         │              ┌─────────┐   ┌───────────┐             │
         │              │  ERROR  │   │  RETRY?   │             │
         │              │ (3 s)   │   └───────────┘             │
         │              └─────────┘         │                   │
         │                    ▲             │ retryable         │ done
         │                    │             ▼                   ▼
         │                    │       re-enter UPLOADING  ┌───────────┐
         │                    │                           │  PLAYING  │──┐
         │                    └───────────────────────────│  Vol+/-   │  │
         │                                                │ = volume  │  │
         │                                                └───────────┘  │
         │                                                       │ done   │
         └───────────────────────────────────────────────────────┘────────┘

    NO_WIFI is an overlay: whenever WiFi.status() != WL_CONNECTED.
    Wake is ignored in NO_WIFI; OLED shows "NO WIFI".
```

**Transitions:**
- `IDLE` → `RECORDING` on Wake-press (only if WiFi connected and no request in flight).
- `RECORDING` → `UPLOADING` on Wake-release OR 60 s cap. `SEND` SFX fires either way.
- `UPLOADING` → `WAITING` after last byte sent. Pentatonic D + start drum bed loop.
- `WAITING` → `DOWNLOADING` on first response byte (note E + stop bed), OR → `ERROR`/`RETRY?` on non-200.
- `DOWNLOADING` → `PLAYING` on body complete (note G).
- `PLAYING` → `IDLE` when playback ends (note A).
- `ERROR` → `IDLE` after 3 s of error text on OLED.
- `RETRY?` → `UPLOADING` after 1 s if the error was retryable and we haven't already retried this utterance; otherwise falls through to `ERROR`. Max one retry per utterance.
- `IDLE` handles Vol+/Vol− as cycle-style.
- `PLAYING` handles Vol+/Vol− as volume-change-and-save.
- All other states ignore Vol+/Vol−.

**Invariants:**
- Recording is abandoned cleanly if WiFi drops: buffer discarded, `sfx_error`, back to `IDLE`.
- Exactly one utterance in flight at a time. Wake is ignored while not in `IDLE`.
- SFX events are always enqueued, never played synchronously — state transitions never block on audio.

## 6. Memory & PSRAM budget

All large buffers are pre-allocated at boot from PSRAM so nothing can fail
mid-utterance.

| Region | Owner | Size | Lifetime |
|---|---|---|---|
| Record buffer (16 kHz mono int16, 60 s) | Audio→Net hand-off | 1.92 MB | Boot; reused per utterance |
| Response buffer (WAV body, ≤ 2 MB) | Net→Audio hand-off | 2.00 MB | Boot; reused per utterance |
| Mixer scratch (int32 sum before clip) | Audio task | 4 KB | Boot |
| SFX sample table (pointers only; data lives in flash) | Audio task | ~0.5 KB | Boot |
| OLED framebuffer (128 × 64 / 8) | UI task | 1 KB (SRAM, not PSRAM) | Boot |
| HTTP multipart prologue/epilogue | Net task | ~4 KB | Per request |
| FreeRTOS stacks + WiFi/LWIP + Arduino core | — | ~4 MB | Runtime |

SFX WAV assets are embedded as `const int16_t[]` arrays in `.rodata` (flash).
Target total ≤ 200 KB.

## 7. Audio pipeline

### 7.1 I2S peripherals

Both buses driven via the Arduino-ESP32 `ESP_I2S` wrapper (falls through to
ESP-IDF `i2s_std`). If the wrapper proves unstable, we drop to raw IDF calls
in the HAL layer without touching callers.

**Input (mic):** BCLK 5 / WS 4 / DIN 6; Philips standard, 16 kHz, 16-bit mono,
left-channel select. DMA 4 × 512 samples = 128 ms total (32 ms per buffer).

**Output (amp):** BCLK 15 / WS 16 / DOUT 7; Philips standard, 16 kHz, 16-bit
mono. DMA 4 × 512 samples = 128 ms total (32 ms per buffer).

Recording never overlaps response playback. The drum-bed loop plays while the
output is active; the input is idle during that time.

### 7.2 Capture path

On `RECORDING` entry the audio task arms the input DMA and writes int16
frames into the record buffer at offset 0, advancing a write cursor. Each DMA
callback computes an RMS, exposed to the UI task via
`std::atomic<uint16_t>` for the VU meter. On `UPLOADING` entry the input DMA
stops and `(ptr, len_bytes)` are pushed to the net task.

### 7.3 Playback path and 2-voice mixer

The mixer is a single function called from the output DMA callback. Two
voices, one shared:

```c
typedef struct {
    const int16_t* data;
    size_t len;
    size_t pos;
    float  gain;
    bool   active;
    bool   loop;
} Voice;

static Voice voices[2];   // 0: SFX + response; 1: bed loop
```

Per DMA frame (512 samples):
```
for i in 0..511:
    acc = 0
    for v in voices if v.active:
        s = v.data[v.pos] * v.gain
        acc += s
        v.pos += 1
        if v.pos >= v.len:
            if v.loop: v.pos = 0
            else:      v.active = false
    out[i] = clip_int16(acc * master_volume)
```

Voice 0 is used for short SFX AND for response-body playback; they are
mutually exclusive in time, so one voice is enough. A response-playback
request pre-empts any in-flight SFX on voice 0 immediately. Voice 1 is
reserved for the bed loop. `master_volume` is the NVS-persisted 0..1 gain.

### 7.4 SFX assets

Embedded via `xxd -i` into `src/audio/sfx_assets.h`. All in canonical format
(16 kHz mono int16). Each is a short sample; authoring fallback is
synthesizing simple sine envelopes if recorded WAVs push the total over
200 KB.

| Symbol | Purpose | Target length |
|---|---|---|
| `sfx_rec_start` | Wake pressed (open hat-style click) | ~80 ms |
| `sfx_rec_send` | Button release or cutoff (kick hit) | ~120 ms |
| `sfx_note_c` … `sfx_note_a` | Pentatonic C D E G A | ~250 ms each |
| `sfx_bed_loop` | Quiet drum loop | ~2 s, loopable |
| `sfx_error` | Descending minor third | ~400 ms |

### 7.5 Event → voice routing

| Event | Voice 0 | Voice 1 |
|---|---|---|
| `REC_START` | play `sfx_rec_start` | — |
| `SEND` (release or cutoff) | play `sfx_rec_send` | — |
| `TCP_CONNECTED` | play `sfx_note_c` | — |
| `UPLOAD_DONE` | play `sfx_note_d` | start `sfx_bed_loop` (loop) |
| `SERVER_FIRST_BYTE` | play `sfx_note_e` | stop |
| `DOWNLOAD_DONE` | play `sfx_note_g` | — |
| `PLAYBACK_START` | play response WAV | — |
| `PLAYBACK_END` | play `sfx_note_a` | — |
| `ERROR` | play `sfx_error` | stop |

## 8. Network / request construction

### 8.1 WiFi lifecycle

`WiFi.begin(WIFI_SSID, WIFI_PASS)` on boot. `WiFi.setAutoReconnect(true)`.
WiFi events drive the `NO_WIFI` overlay via `WiFi.onEvent` callbacks; no
polling. DNS resolution happens at the first request and the resolved
`IPAddress` is cached until a disconnect.

### 8.2 Multipart POST (hand-rolled)

Arduino `HTTPClient` has no multipart encoder. The body layout is static, so
we assemble it manually and send it in three pieces over the underlying
`WiFiClient` without copying the 1.9 MB audio payload:

```
POST /v1/restyle HTTP/1.1
Host: <SERVER_HOST>
User-Agent: xh-s3e-ai/0.1
Content-Type: multipart/form-data; boundary=XHS3E-<request_id>
Content-Length: <computed>
X-Request-Id: <uuid-v4 generated on device>

--XHS3E-<request_id>
Content-Disposition: form-data; name="style_id"

<current_style_id>
--XHS3E-<request_id>
Content-Disposition: form-data; name="audio"; filename="utterance.wav"
Content-Type: audio/wav

<wav_header (44 B)><PCM bytes from record buffer>
--XHS3E-<request_id>
Content-Disposition: form-data; name="language"

en
--XHS3E-<request_id>--
```

- The 44-byte RIFF/WAVE header is pre-computed for 16 kHz / 16-bit / mono
  with placeholder size fields; the two size fields are patched with the
  actual record length just before send.
- The multipart prologue and epilogue are built into a single ~500-byte
  buffer on each request.
- Send order on the raw socket: prologue bytes → PCM bytes from record
  buffer → epilogue bytes.

### 8.3 Response parsing

1. Read status line.
2. Read headers into a fixed 2 KB buffer (reject if exceeded).
3. Required checks:
   - `Content-Type` starts with `audio/wav`, otherwise treat body as JSON error.
   - `Content-Length` present and ≤ 2 MB.
4. Captured headers: `X-Transcript`, `X-Restyled-Text` (URL-decoded and
   stashed for future OLED scrolling), `X-Request-Id` (echoed to serial log).
5. Read exactly `Content-Length` bytes into the response buffer.
6. Verify the first 12 bytes are `RIFF....WAVE`, parse `fmt ` and `data`
   chunks, and expose `(pcm_ptr, pcm_len)` to the audio task.

### 8.4 Timeouts

Two layers: per-phase stall detectors and one overall wall-clock cap. The
total wall-clock cap is the hard limit; per-phase timeouts are for detecting
a hung stage sooner when the total hasn't yet been reached.

- Connect (TCP): 5 s
- Upload socket write (no progress): 10 s
- Server response-start (time from last upload byte to first response byte): 45 s — sized to the server's 45 s pipeline ceiling
- Download (no bytes received for): 10 s sliding window
- **Total pipeline wall clock cap: 55 s.** Past that, abort with `client_timeout`.

A long run that hits every per-phase maximum would exceed 55 s; the wall-clock
cap wins. In practice the steady state is well under 10 s total.

### 8.5 Styles and health API

- `GET /v1/styles` called once at boot; result cached in RAM and used to
  drive the Vol+/Vol− cycle. Retry every 30 s on failure.
- `GET /healthz` polled every 60 s during `IDLE` only; results shown as
  status dots in the OLED footer.

## 9. OLED UI

SSD1306 over I2C at 400 kHz. `Adafruit_SSD1306` + `Adafruit_GFX`. Full
framebuffer redraw on state change; 20 Hz VU meter updates during
`RECORDING` and `PLAYING`.

### 9.1 Layout (128 × 64)

```
┌──────────────────────────────────────────────────────┐
│ [*-_] JESUS                              .oO WiFi    │  header  (10 px)
├──────────────────────────────────────────────────────┤
│                                                      │
│              READY                                   │  primary (16 px, size 2)
├──────────────────────────────────────────────────────┤
│ ▮▮▮▮▮▮▯▯▯▯▯▯▯▯▯▯▯▯▯▯                                 │  secondary (8 px)
├──────────────────────────────────────────────────────┤
│ stt ok · llm ok · tts ok                             │  footer (20 px)
└──────────────────────────────────────────────────────┘
```

### 9.2 Per-state rendering

| State | Header | Primary | Secondary | Footer |
|---|---|---|---|---|
| `IDLE` | style name + WiFi bars | `READY` | blank | health dots; `vol: ▮▮▮▮▮▮▯▯▯▯` for 1 s after any vol change |
| `RECORDING` | style name + WiFi bars | `REC mm:ss` | VU meter from mic RMS | blank |
| `UPLOADING` | style name + WiFi bars | `SENDING` | progress bar (bytes sent / total) | blank |
| `WAITING` | style name + WiFi bars | `THINKING` | 2 px indeterminate sweeper | blank |
| `DOWNLOADING` | style name + WiFi bars | `RECEIVING` | progress bar (bytes received / Content-Length) | blank |
| `PLAYING` | style name + WiFi bars | `PLAYING` | VU meter from output RMS | `vol: ▮▮▮▮▮▮▯▯▯▯` for 1 s after vol change |
| `ERROR` | style name + WiFi bars | `ERROR` | blank | `<error_code>` (wraps to two lines) |
| `NO_WIFI` | `NO WIFI` + flashing icon | blank | blank | `ssid: <configured>` |

### 9.3 VU meter

Source is the last DMA-block RMS, converted to 0..20 blocks on a log scale
so quiet speech is visible without clipping. Audio task writes
`std::atomic<uint16_t>`; UI task reads at 20 Hz.

### 9.4 Style list handling

If `GET /v1/styles` fails at boot, firmware uses the NVS-saved
`style_id` (or `DEFAULT_STYLE` if none), shows `styles: offline` in the
footer, and retries the call every 30 s. When the call succeeds, if the
saved `style_id` is absent from the returned list, clamp `style_idx` to 0
and persist.

## 10. Config, NVS, and secrets

### 10.1 Compile-time (`include/secrets.h`, gitignored)

```c
#pragma once
#define WIFI_SSID      "..."
#define WIFI_PASS      "..."
#define SERVER_HOST    "192.168.x.x"
#define SERVER_PORT    8080
#define DEFAULT_STYLE  "jesus"
#define FALLBACK_LANG  "en"
```

A committed `include/secrets.h.example` documents the format.

### 10.2 Runtime (NVS namespace `xhs3e`)

| Key | Type | Default | Updated when |
|---|---|---|---|
| `style_idx` | uint16 | 0 | Style cycled in `IDLE` (debounced) |
| `style_id` | string ≤ 32 | `DEFAULT_STYLE` | Alongside `style_idx` as a resilience fallback |
| `volume_x10` | uint8 (0..10) | 6 | Vol+/Vol− in `PLAYING` (debounced) |

Both write paths rate-limit to ≤ 1 flash write per second per key so rapid
button mashes don't wear the part.

### 10.3 Logging

Single format, 115200 baud:
```
[  12.345] INFO  net  upload_done  bytes=491520 dur_ms=312 rid=a1b2c3d4
[  12.678] INFO  net  server_first_byte dur_ms=333 rid=a1b2c3d4
[  18.901] WARN  audio mixer_underrun voices=2
```
`[timestamp] LEVEL task message key=value ...`. `X-Request-Id` is echoed so
device logs and `docker compose logs` on the server can be correlated.

## 11. Error handling and retry

### 11.1 Server error codes (v1 handled subset)

Server errors arrive as JSON on any non-200 response:
`{error_code, message, retryable}`. We show `error_code` on the OLED for 3 s
and play `sfx_error`.

| Server `error_code` | Display | Retry |
|---|---|---|
| `invalid_audio` | `invalid_audio` | no |
| `unknown_style` | `unknown_style` | no |
| `audio_too_long` | `too_long` | no |
| `no_speech` | `no_speech` | no |
| `lang_mismatch` | `lang_mismatch` | no |
| `llm_empty` | `llm_empty` | yes (once) |
| `stt_unavailable` | `stt_down` | yes (once) |
| `llm_unavailable` | `llm_down` | yes (once) |
| `tts_unavailable` | `tts_down` | yes (once) |
| `pipeline_timeout` | `svr_timeout` | yes (once) |
| `internal_error` | `svr_error` | no |

### 11.2 Client-side errors

| Condition | Display | Retry |
|---|---|---|
| WiFi disconnect mid-record/upload/download | `no_wifi` | no (wait for reconnect) |
| DNS / TCP failure | `net_fail` | yes (once) |
| Response headers > 2 KB | `hdr_overflow` | no |
| Missing / invalid `Content-Length` | `bad_resp` | no |
| `Content-Length` > 2 MB | `too_big` | no |
| Body not a valid WAV | `bad_wav` | no |
| 55 s pipeline cap exceeded | `client_timeout` | no |

### 11.3 Retry policy

Exactly one retry per utterance, triggered only for retryable failures
above. 1 s delay before re-entering `UPLOADING`. After the retry, any
failure (including a second retryable one) falls through to `ERROR`.

## 12. Repository layout

```
live-speech-style-esp32/
├── platformio.ini
├── partitions.csv                  # default_16MB.csv, copied locally for clarity
├── include/
│   ├── secrets.h                   # gitignored
│   └── secrets.h.example
├── src/
│   ├── main.cpp                    # setup(), loop() — delegates to tasks
│   ├── app_state.{h,cpp}           # AppState enum, mutex, transitions
│   ├── tasks/
│   │   ├── ui_task.{h,cpp}
│   │   ├── net_task.{h,cpp}
│   │   └── audio_task.{h,cpp}
│   ├── hal/
│   │   ├── buttons.{h,cpp}
│   │   ├── i2s_in.{h,cpp}
│   │   ├── i2s_out.{h,cpp}
│   │   └── oled.{h,cpp}
│   ├── net/
│   │   ├── http_client.{h,cpp}
│   │   ├── response_parser.{h,cpp}
│   │   └── styles_api.{h,cpp}
│   ├── audio/
│   │   ├── mixer.{h,cpp}
│   │   ├── sfx.{h,cpp}
│   │   └── sfx_assets.h            # generated from xxd -i
│   ├── config/
│   │   ├── nvs_store.{h,cpp}
│   │   └── wav_header.{h,cpp}
│   └── util/
│       ├── log.{h,cpp}
│       └── request_id.{h,cpp}
├── assets/
│   └── sfx/                        # source WAVs (committed)
├── tools/
│   └── bake_sfx.py                 # resample/trim → int16 C arrays → sfx_assets.h
├── test/                           # PlatformIO native unit tests
│   ├── test_wav_header/
│   ├── test_mixer/
│   ├── test_response_parser/
│   └── test_state_machine/
├── docs/
│   ├── 2026-04-22-live-speech-style-design.md
│   └── 2026-04-23-firmware-design.md
└── README.md
```

## 13. Build configuration

```ini
[env:xh-s3e-ai]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB
board_build.partitions = default_16MB.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=3
lib_deps =
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
    bblanchon/ArduinoJson
```

`HTTPClient`, `WiFi`, and `Preferences` ship with the Arduino-ESP32 core and
are not listed as dependencies.

## 14. Testing

- **Native unit tests** (PlatformIO `platform = native`) for everything with
  no hardware dependency: mixer math, WAV header patch, response parser,
  state-machine transitions, NVS helper with a fake `Preferences`. Fast,
  runs on laptop.
- **On-device smoke tests** via a `-DSMOKE_BUILD` flag: boots into a
  sequence that plays each SFX, renders each OLED screen, and does one
  loopback round-trip to the server without waiting for button input.
  Used for bring-up and regression checks after firmware changes.
- **No formal CI** for v1. Manual local builds.

## 15. Future work (explicitly deferred)

- TLS / auth / cert pinning.
- OTA firmware updates.
- Captive-portal or BLE WiFi provisioning.
- mDNS server discovery.
- Transcript scrolling on the OLED (`X-Transcript` + `X-Restyled-Text`).
- Streaming upload-while-recording (removes the PSRAM record-buffer ceiling).
- Streaming playback-while-downloading (cuts perceived latency).
- 3+ voice mixer (allow SFX to overlap response audio).
- Stock Xiaozhi firmware protocol compatibility.
- Multi-device coordination against one server.
