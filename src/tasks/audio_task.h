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
bool audio_task_consume_playback_end();   // returns true once after a response WAV finishes playing
