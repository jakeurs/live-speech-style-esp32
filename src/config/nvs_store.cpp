#include "config/nvs_store.h"
#ifdef UNIT_TEST
  #include "preferences_fake.h"
  static Preferences g_prefs;
#else
  #include <Preferences.h>
  static Preferences g_prefs;
#endif
#include <cstring>

namespace {
NvsClock g_clock = nullptr;
uint32_t g_last_save_ms = 0;
}

void nvs_set_clock_ms(NvsClock c) { g_clock = c; g_last_save_ms = 0; }

void nvs_test_reset() {
#ifdef UNIT_TEST
    g_prefs.strs.clear();
    g_prefs.u32s.clear();
#endif
    g_last_save_ms = 0;
    g_clock = nullptr;
}

NvsState nvs_load() {
    NvsState s;
    g_prefs.begin("xhs3e", true);
    s.style_idx   = g_prefs.getUShort("style_idx", 0);
    String sid    = g_prefs.getString("style_id", "jesus");
    strncpy(s.style_id, sid.c_str(), sizeof(s.style_id) - 1);
    s.volume_x10  = g_prefs.getUChar("vol_x10", 6);
    g_prefs.end();
    return s;
}

bool nvs_save(const NvsState& s) {
    uint32_t now = g_clock ? g_clock() : 0;
    if (g_last_save_ms != 0 && now - g_last_save_ms < 1000) return false;
    g_prefs.begin("xhs3e", false);
    g_prefs.putUShort("style_idx", s.style_idx);
    g_prefs.putString("style_id", s.style_id);
    g_prefs.putUChar("vol_x10", s.volume_x10);
    g_prefs.end();
    g_last_save_ms = now;
    return true;
}
