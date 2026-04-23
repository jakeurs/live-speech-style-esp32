#pragma once
#include <cstdint>
#include <cstddef>

constexpr int MIXER_VOICES = 2;

void mixer_reset();
void mixer_play(int voice, const int16_t* data, size_t len, float gain, bool loop);
void mixer_stop(int voice);
bool mixer_voice_active(int voice);
void mixer_render(int16_t* out, size_t frames, float master_volume);
