#pragma once
#include <cstdint>
#include <cstddef>

struct StyleList { char ids[16][33]; char names[16][33]; uint8_t count; };
struct HealthStatus { bool stt_ok, llm_ok, tts_ok; };
