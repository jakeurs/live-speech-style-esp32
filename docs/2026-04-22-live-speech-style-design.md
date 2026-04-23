# Live Speech Style Transfer — Design

**Status:** Draft, pending user review
**Date:** 2026-04-22
**Target hardware:** RTX 4060 Ti 16 GB (server), Xiaozhi ESP32-S3-WROOM-1-N16R8 with custom firmware (client)

## 1. Goal

A local, utterance-based speech restyling service. A user speaks into a Xiaozhi ESP32-S3 device; the device uploads the utterance; the service transcribes it, rewrites it in a preset "style" (e.g., Jesus, pirate), synthesizes the result with a style-specific voice, and returns the audio. The ESP32 plays the response through its I2S speaker.

All models run locally on a single RTX 4060 Ti 16 GB. Open-source only.

## 2. Scope and non-goals

**In scope:**
- Utterance-based HTTP request/response (push-to-talk style).
- Preset styles defined by YAML files on disk, with voice cloning via reference WAVs.
- Single-user, LAN-only deployment.
- Docker-compose orchestration of four services on one host.
- LangGraph-based pipeline with conditional guardrails (input validation, output guarding).

**Out of scope for v1 (flagged YAGNI, not objections):**
- Streaming audio / WebSocket transport.
- Free-form style prompts from the client.
- Authentication, rate limiting, persistent storage, caching.
- Multi-GPU, model hot-swapping at request time.
- ESP32 firmware (separate project).
- Stock Xiaozhi firmware compatibility (user is writing custom firmware).

## 3. Architecture

Four containers orchestrated by one `docker-compose.yml`. Three GPU containers share the single 4060 Ti via NVIDIA Container Toolkit; one CPU-only container orchestrates.

```
ESP32-S3 ──HTTP──▶  api  ──HTTP──▶  stt   (faster-whisper large-v3 int8)
                     │    ──HTTP──▶  llm   (Ollama, Qwen3 4B)
                     │    ──HTTP──▶  tts   (XTTS-v2)
                     │
                     └─ LangGraph orchestrator
                        Style registry (YAML)
```

**Services:**

1. **`api`** (CPU-only). FastAPI + LangGraph. The only service the ESP32 talks to. Loads style YAMLs on startup. Runs the orchestration graph. Calls the three model services internally.
2. **`stt`** (GPU). Thin FastAPI wrapper around `faster-whisper large-v3` (int8_float16). Resident model. Exposes `POST /transcribe`.
3. **`llm`** (GPU). Stock `ollama/ollama` image. Pulls `qwen3:4b-instruct-2507` (or configured model) on first run. OpenAI-compat API on `:11434`.
4. **`tts`** (GPU). Thin FastAPI wrapper around `coqui-tts` (XTTS-v2). Resident model. Exposes `POST /synthesize`. Reference voices mounted from `./styles/voices`.

**Why four containers instead of one:** XTTS, Whisper, and Ollama have conflicting Python/CUDA environments. Separation gives each a clean dep tree, independent rebuilds, and a single clear purpose per service.

## 4. VRAM budget

All three GPU models stay resident. No swapping.

| Component | VRAM |
|---|---|
| faster-whisper large-v3 int8_float16 | ~1.5 GB |
| Qwen3 4B Instruct Q4_K_M (Ollama) | ~3.0 GB |
| XTTS-v2 | ~4.0 GB |
| CUDA contexts ×3 + activations + KV cache | ~2.0 GB |
| **Total resident** | **~10.5 GB** |
| **Headroom** | **~5.5 GB** |

Headroom supports swapping to a larger LLM (up to ~12B Q4) without rearchitecting.

## 5. API contract (ESP32-facing)

### `POST /v1/restyle`

```
Content-Type: multipart/form-data
Form fields:
  style_id:  string   (required — filename stem, e.g. "jesus")
  audio:     file     (required — WAV, 16 kHz, mono, 16-bit PCM)
  language:  string   (optional — ISO 639-1, default "en")
```

**Response 200:**
```
Content-Type: audio/wav
Body: WAV, 16 kHz, mono, 16-bit PCM
Headers:
  X-Transcript:      <utf-8, url-encoded — recognized text>
  X-Restyled-Text:   <utf-8, url-encoded — LLM output>
  X-Style-Id:        <echoed>
  X-Processing-Ms:   <total wall-clock ms>
  X-Request-Id:      <correlation ID>
```

**Response 4xx/5xx:**
```
Content-Type: application/json
Body: {"error_code": "...", "message": "...", "retryable": bool}
```

### Supporting endpoints

- `GET /v1/styles` — JSON list of `{id, name, description}`.
- `GET /v1/styles/{id}` — single style metadata (no prompts, no voice paths).
- `GET /healthz` — aggregate health of stt/llm/tts.
- `POST /admin/reload-styles` — localhost-bound, re-scans the styles directory.

### Size and rate constraints

- **Input audio hard ceiling:** 60 s (413 on overflow).
- **Input audio recommended ceiling (ESP32 side):** 30 s — matches Whisper's native mel window and keeps round-trip under ~10 s.
- **Output audio ceiling:** ~25 s (XTTS quality degrades beyond this; enforced by truncating `restyled_text` at ~800 chars at a sentence boundary).

### Transport rationale

- **Multipart HTTP** — ESP-IDF `esp_http_client` supports multipart natively; binary body would save bytes but lose the `style_id` field.
- **WAV at 16 kHz both directions** — matches I2S defaults on device; zero resampling on the ESP32.
- **Synchronous response, full body** — ESP32 buffers response in PSRAM and plays from there; simpler than chunked streaming.
- **No auth** — LAN-only. Add shared-secret middleware later if exposed externally.

## 6. Style YAML schema

One file per style at `./styles/<style_id>.yml`. Filename stem is the `style_id`. Voice reference clips live at `./styles/voices/<file>.wav`.

```yaml
# styles/jesus.yml
name: "Jesus of Nazareth"
description: "Parables, beatitudes, first-century Judean rabbi register."

prompt: |
  Rewrite the user's text as if spoken by Jesus of Nazareth in the
  King James register. Favor parables, beatitudes, and "verily I say
  unto thee" constructions. Preserve the original meaning. Do not
  invent new facts. Output ONLY the restyled text — no preamble,
  no quotation marks, no commentary.

voice:
  reference_wav: "voices/jesus.wav"   # 6-10s clean speech, 24 kHz mono
  language: "en"                      # XTTS speaker language
  speed: 0.9                          # optional, default 1.0
  temperature: 0.7                    # XTTS sampling temp, default 0.75

llm:
  temperature: 0.8                    # optional, overrides service default
  max_tokens: 300                     # optional
```

**Schema rules:**

- Required: `name`, `prompt`, `voice.reference_wav`.
- Optional: everything else, with defaults.
- Validated at load time via Pydantic. Invalid files are logged and skipped, not fatal.
- Hot reload via `POST /admin/reload-styles` (localhost-bound) or SIGHUP.
- Expected soft cap: ~100 styles. Real count expected much lower.

**Prompt merging:** the api composes OpenAI-style chat messages:
```
system: <style.prompt>
user:   <transcript>
```
Sent to Ollama, which Qwen3 handles natively.

**Voices directory:** separate from YAML to allow gitignoring binary files or managing via Git LFS independently.

## 7. LangGraph flow

Five nodes, three conditional edges. State is a Pydantic model carried through the graph.

```
load_style ─▶ transcribe ─▶ input_validate ─▶ restyle ─▶ output_guard ─▶ synthesize ─▶ END

Conditional edges (all terminate at END with error set):
  load_style:       style_id not found                   → 404 unknown_style
  input_validate:   empty transcript                     → 422 no_speech
                    transcript < 3 chars                 → 422 no_speech
                    detected_lang ≠ style.voice.language → 422 lang_mismatch
  output_guard:     empty LLM output                     → 422 llm_empty
                    over 800 chars                       → truncate at sentence, continue
```

### State

```python
class RestyleState(BaseModel):
    style_id: str
    audio_in: bytes
    language_hint: str | None
    style: StyleConfig | None
    transcript: str | None
    detected_lang: str | None
    restyled_text: str | None
    audio_out: bytes | None
    error: ErrorInfo | None
    timings: dict[str, float]
```

### Why LangGraph (and not a plain function)

- **Conditional edges** give clean syntax for three early-exits vs. nested if/else.
- **Per-node timing hooks** populate `X-Processing-Ms` for free.
- **Extension points** for future nodes (cache lookup, safety classifier) without restructuring.
- **LangSmith** tracing is zero-config when/if enabled.

### Retries

None at graph level for v1. Transport failures bubble to 502; client decides whether to retry based on the `retryable` flag.

## 8. Audio formats (canonical)

| Stage | Format | Rate | Channels | Bit depth |
|---|---|---|---|---|
| ESP32 mic → api | WAV | 16 kHz | mono | 16-bit PCM |
| api → stt | WAV or raw PCM | 16 kHz | mono | 16-bit |
| stt internal | float32 | 16 kHz | mono | — |
| llm | text only | — | — | — |
| tts synthesis (native) | float32 | 24 kHz | mono | — |
| tts service output | WAV | 16 kHz | mono | 16-bit PCM |
| api → ESP32 | WAV | 16 kHz | mono | 16-bit PCM |
| Reference voice clips | WAV | 24 kHz | mono | 16-bit PCM |

Server-side resample of XTTS output (24 → 16 kHz) happens inside the tts service via `scipy.signal.resample_poly`. Negligible latency (~5 ms for 5 s audio).

## 9. Error handling

### Error taxonomy

| HTTP | `error_code` | Cause | `retryable` |
|---|---|---|---|
| 400 | `invalid_audio` | Unparseable WAV, wrong rate/channels, corrupt | false |
| 404 | `unknown_style` | `style_id` not in registry | false |
| 413 | `audio_too_long` | > 60 s | false |
| 422 | `no_speech` | Whisper returned empty transcript | false |
| 422 | `lang_mismatch` | Detected language ≠ style language | false |
| 422 | `llm_empty` | LLM returned empty string | true |
| 502 | `stt_unavailable` | stt container down or timed out | true |
| 502 | `llm_unavailable` | llm container down or timed out | true |
| 502 | `tts_unavailable` | tts container down or timed out | true |
| 504 | `pipeline_timeout` | Total wall clock > 45 s | true |
| 500 | `internal_error` | Unexpected | maybe |

### Timeouts (wall clock per stage)

- STT: 10 s
- LLM: 15 s
- TTS: 20 s
- Total pipeline ceiling: 45 s (enforced via `asyncio.wait_for` at the api layer)

Per-stage timeouts are tuned so that even a worst-case run fits inside the outer 45 s cap. Steady state is under 10 s total.

### Logging

Every request gets an `X-Request-Id` correlation header (generated if absent). Every service logs the ID on every line. Logs are JSON lines to stdout. One `grep` across `docker compose logs` reconstructs a trace.

### Metrics

Optional Prometheus `/metrics` endpoint on the api, gated by `ENABLE_METRICS=true`. Exposes per-node latency histograms and per-`error_code` counters.

## 10. Repository layout

```
live-speech-style/
├── docker-compose.yml
├── .env                      # API_PORT, LLM_MODEL, WHISPER_MODEL, STYLES_DIR...
├── docs/
│   └── superpowers/specs/
│       └── 2026-04-22-live-speech-style-design.md
├── services/
│   ├── api/
│   │   ├── Dockerfile
│   │   ├── pyproject.toml
│   │   └── src/
│   │       ├── main.py       # FastAPI app, endpoint handlers
│   │       ├── graph.py      # LangGraph definition
│   │       ├── nodes/        # one file per node
│   │       ├── styles.py     # YAML loader + in-memory registry
│   │       └── clients/      # stt/llm/tts HTTP clients
│   ├── stt/
│   │   ├── Dockerfile
│   │   ├── pyproject.toml
│   │   └── src/main.py
│   ├── tts/
│   │   ├── Dockerfile
│   │   ├── pyproject.toml
│   │   └── src/main.py
│   └── llm/
│       └── bootstrap.sh      # one-shot ollama pull ${LLM_MODEL}
├── styles/
│   ├── jesus.yml
│   ├── pirate.yml
│   └── voices/
│       ├── jesus.wav
│       └── pirate.wav
├── tests/
│   ├── unit/                 # per-node tests with mocked clients
│   ├── integration/          # full compose round-trip
│   └── fixtures/audio/       # 3-4 golden WAV clips
└── pyproject.toml            # root dev tooling (lint, fmt, test)
```

## 11. Docker compose

```yaml
services:
  api:
    build: ./services/api
    env_file: .env
    ports: ["${API_PORT:-8080}:8080"]
    volumes:
      - ./styles:/styles:ro
    depends_on: [stt, llm, tts, llm-bootstrap]

  stt:
    build: ./services/stt
    env_file: .env
    deploy: {resources: {reservations: {devices: [{capabilities: [gpu]}]}}}
    volumes:
      - whisper_cache:/root/.cache/huggingface

  llm:
    image: ollama/ollama:latest
    env_file: .env
    deploy: {resources: {reservations: {devices: [{capabilities: [gpu]}]}}}
    volumes:
      - ollama_data:/root/.ollama

  llm-bootstrap:
    image: ollama/ollama:latest
    depends_on: [llm]
    entrypoint: ["/bin/sh", "-c"]
    command: ["ollama pull ${LLM_MODEL}"]
    environment:
      OLLAMA_HOST: http://llm:11434
    restart: "no"

  tts:
    build: ./services/tts
    env_file: .env
    deploy: {resources: {reservations: {devices: [{capabilities: [gpu]}]}}}
    volumes:
      - ./styles/voices:/voices:ro
      - xtts_cache:/root/.cache/tts

volumes:
  whisper_cache:
  ollama_data:
  xtts_cache:
```

**Config knobs** (via `.env`):
- `API_PORT` — default 8080
- `LLM_MODEL` — default `qwen3:4b-instruct-2507`
- `WHISPER_MODEL` — default `large-v3`
- `TTS_LANGUAGE` — default `en`
- `STYLES_DIR` — default `/styles`
- `ENABLE_METRICS` — default `false`

## 12. Testing

- **Unit tests** (api container): pure-Python tests against LangGraph nodes with mocked stt/llm/tts clients. Fast (<1 s per test). Coverage: YAML parsing, each conditional edge, error mapping, header encoding.
- **Integration tests** (docker-compose): `pytest` drives the full compose stack against fixture WAVs. Covers full round-trip for 2-3 styles end-to-end. Slow (~30 s per test) — CI only, not every commit.
- **Golden audio fixtures:** 3-4 short recordings (5-10 s each, public-domain source) checked into `tests/fixtures/audio/`. Assertions cover (transcript contains expected substring, non-empty output WAV with duration > 0). No bit-exactness assertions on TTS output (XTTS is stochastic).
- **No ESP32 tests** in this repo.

## 13. Future work (explicitly deferred)

- Streaming / WebSocket transport (would remove the PSRAM audio-length ceiling).
- Stock Xiaozhi firmware protocol compatibility (Opus + WebSocket + Xiaozhi message schema).
- Input-hash caching of `(audio_sha256, style_id) → output_wav`.
- Safety classifier node between `restyle` and `synthesize`.
- Multi-turn conversational state.
- Free-form style override field on the API.
