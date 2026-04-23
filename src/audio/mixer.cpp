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
