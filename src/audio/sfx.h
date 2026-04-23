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
void sfx_apply(const SfxCmd& c);
