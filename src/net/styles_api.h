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
