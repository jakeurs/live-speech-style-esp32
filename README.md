# live-speech-style-esp32

Firmware for the XH-S3E-AI_v1.0 Mini (ESP32-S3-WROOM-1-N16R8) — a push-to-talk
client for the [live-speech-style](docs/2026-04-22-live-speech-style-design.md)
server. Hold the Wake button, speak, release; the device uploads your utterance,
the server transcribes → restyles → resynthesizes it in a chosen voice (Jesus,
pirate, etc.), and the response plays back through the on-board speaker.

## Hardware

| Peripheral | Function | GPIO |
|---|---|---|
| OLED (SSD1306, I2C) | SDA / SCL | 41 / 42 |
| Mic (I2S in) | WS / BCLK / DIN | 4 / 5 / 6 |
| Amp (I2S out) | DOUT / BCLK / WS | 7 / 15 / 16 |
| Wake / push-to-talk | button | 0 |
| Vol+ / next style | button | 40 |
| Vol− / prev style | button | 39 |
| Reset | button | EN / RST |

## Setup

1. Install [PlatformIO](https://platformio.org/) (via VS Code extension or `pipx install platformio`).

2. Copy `include/secrets.h.example` to `include/secrets.h` and fill in your values:
   ```cpp
   #define WIFI_SSID      "..."
   #define WIFI_PASS      "..."
   #define SERVER_HOST    "192.168.x.x"
   #define SERVER_PORT    8080
   ```
   `include/secrets.h` is gitignored.

3. (Optional) Replace the synthesized SFX with real WAV samples.
   Drop `{rec_start,rec_send,note_{c,d,e,g,a},bed_loop,error}.wav` (16 kHz
   mono 16-bit PCM) into `assets/sfx/` then run:
   ```
   python tools/bake_sfx.py
   ```
   Commit the regenerated `src/audio/sfx_assets.h`.

4. Build and flash:
   ```
   pio run -e xh-s3e-ai -t upload -t monitor
   ```

## Usage

- **Boot:** OLED shows the selected style in yellow + `READY` in blue. Serial
  logs WiFi connection and styles-list fetch.
- **Record + send:** hold Wake; speak; release. Hard cap at 60 s.
- **Cycle styles:** tap Vol+/Vol− in IDLE. Persists across reboot via NVS.
- **Change volume:** tap Vol+/Vol− while the response is playing. Also persists.
- **On error:** error code shows for 3 s, then returns to IDLE. Retryable
  server errors auto-retry once after a 1 s delay.

## Tests

Native unit tests (no hardware, laptop only):
```
pio test -e native
```
Covers the pure-logic modules: log formatting, request IDs, WAV header,
2-voice mixer, HTTP response parser, state machine, NVS store, JSON parsing.

Hardware smoke testing is ad-hoc — there's no CI harness.

## Notable design decisions and gotchas

- **USB CDC:** `platformio.ini` sets `-DARDUINO_USB_CDC_ON_BOOT=1` so `Serial`
  output actually appears on the USB CDC endpoint. Without that flag `Serial`
  binds to UART0 on GPIO 43/44 which aren't wired to the connector on this board.
- **Legacy ESP-IDF I2S driver:** The installed arduino-esp32 2.x core does not
  ship the new `ESP_I2S.h` (added in 3.x). The HAL uses `driver/i2s.h` directly
  — slightly more boilerplate but stable across this toolchain.
- **OLED text sizing:** the 0.96" SSD1306 on this board renders size-1 (6×8)
  text illegibly. The UI uses size-2 (12×16) throughout. This caps the
  horizontal budget at ~10 chars per line, which is why style names are
  truncated to 10 chars and error codes are short like `svrtimeout` rather
  than `pipeline_timeout`.
- **Yellow/blue seam:** the dual-color panel transitions from yellow to blue
  at row 19 (not 16). The UI places the style name in the top 16-row band so
  it lands fully in yellow.
- **No work in WiFi event callbacks:** the `arduino_events` task has a tiny
  (~4 KB) stack. Anything that wants to do HTTP, PSRAM allocation, or JSON
  parsing in response to WiFi events must defer to the main loop or a
  dedicated task. The styles-fetch at boot runs from `loop()`; the net task
  owns its own 8 KB stack.
- **PSRAM buffers:** 1.92 MB record + 2 MB response buffer are
  `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`'d at boot. If you ever see a
  `psram_alloc_fail` log at boot, confirm `board_build.arduino.memory_type
  = qio_opi` is set in `platformio.ini`.

## Troubleshooting

- **Boot loop or "A stack overflow in task arduino_events":** something in a
  WiFi event callback is doing too much work. See the gotcha above.
- **OLED shows nothing after flash:** check that `oled_begin()` returns true
  on serial — address might be 0x3D instead of 0x3C on some clones.
- **Mic RMS stays at 0:** the mic may be PDM rather than standard I2S. The
  current `i2s_in.cpp` uses `I2S_COMM_FORMAT_STAND_I2S`; for PDM you'd need
  `I2S_MODE_PDM` on RX.
- **WiFi never associates:** ESP32-S3 is 2.4 GHz only. Confirm the SSID in
  `secrets.h` is the 2.4 GHz band, and the password is exact (case-sensitive).
- **`unknown_style` error on every send:** the NVS-saved style_id is out of
  sync with the server's style list. The boot clamp resets it to index 0 if
  the saved id isn't in the list — but only once `GET /v1/styles` succeeds.

## Design documents

- [Server design](docs/2026-04-22-live-speech-style-design.md)
- [Firmware design](docs/2026-04-23-firmware-design.md)
- [Implementation plan](docs/2026-04-23-firmware-implementation-plan.md)
