#pragma once

#include <stddef.h>
#include <stdint.h>

// Device-local display and sound prefs. Not part of config.tar: they are
// written by the Admin panel and must survive a configuration import. Clamp
// and JSON live here (pure logic) so the native tests cover them; the SD
// round-trip is storage/device_prefs_store.

constexpr uint8_t DEVICE_PREFS_BRIGHTNESS_MIN = 10;  // never black out the panel
constexpr uint8_t DEVICE_PREFS_BRIGHTNESS_MAX = 100;
constexpr uint8_t DEVICE_PREFS_BRIGHTNESS_DEFAULT = 100;

constexpr uint8_t DEVICE_PREFS_BEEP_MIN = 0;  // 0 = mute
constexpr uint8_t DEVICE_PREFS_BEEP_MAX = 100;
constexpr uint8_t DEVICE_PREFS_BEEP_DEFAULT = 90;

// ES8311 DAC volume (reg 0x32): 0x00 = mute / -95.5 dB, 0xBF = 0 dB.
constexpr uint8_t DEVICE_PREFS_BEEP_DAC_MAX = 0xBF;

typedef struct {
    uint8_t brightness;   // 10-100
    uint8_t beep_volume;  // 0-100
} device_prefs_t;

void device_prefs_defaults(device_prefs_t* out);
void device_prefs_clamp(device_prefs_t* p);

uint8_t device_prefs_clamp_brightness(int v);
uint8_t device_prefs_clamp_beep_volume(int v);

// Linear map onto the ES8311 DAC register. 0 stays 0 (mute); 100 is 0 dB.
uint8_t device_prefs_beep_dac(uint8_t pct);

// Fills `out` with defaults, then overlays any recognized keys. Returns false
// when `json` is not an object (caller should keep defaults). Missing keys
// stay at their defaults; out-of-range values are clamped.
bool device_prefs_parse(const char* json, size_t len, device_prefs_t* out);

// Compact object, no extra whitespace. Returns bytes written (not counting
// the NUL). 0 if `out` is too small.
size_t device_prefs_format(const device_prefs_t* p, char* out, size_t cap);
