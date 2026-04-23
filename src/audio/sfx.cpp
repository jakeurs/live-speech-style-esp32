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
