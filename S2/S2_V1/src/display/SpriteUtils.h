#pragma once
// ─── Diagnóstico de asignación de sprites ─────────────────────
#include <LovyanGFX.hpp>
#include <esp_heap_caps.h>   // heap_caps_check_integrity_all

static inline void _logSpriteAlloc(const char* name, LGFX_Sprite& spr) {
    void* buf = spr.getBuffer();
    // esp_ptr_external_ram detecta PSRAM en ESP32-S2
    bool inPsram = (buf != nullptr) && esp_ptr_external_ram(buf);
    log_i("%-10s %dx%d  buf=%p  psram=%s",
          name, spr.width(), spr.height(), buf,
          inPsram ? "SI" : "NO");
}
