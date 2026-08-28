#include "app/device_prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void device_prefs_defaults(device_prefs_t* out) {
    if (!out) return;
    out->brightness = DEVICE_PREFS_BRIGHTNESS_DEFAULT;
    out->beep_volume = DEVICE_PREFS_BEEP_DEFAULT;
}

uint8_t device_prefs_clamp_brightness(int v) {
    if (v < (int)DEVICE_PREFS_BRIGHTNESS_MIN) return DEVICE_PREFS_BRIGHTNESS_MIN;
    if (v > (int)DEVICE_PREFS_BRIGHTNESS_MAX) return DEVICE_PREFS_BRIGHTNESS_MAX;
    return (uint8_t)v;
}

uint8_t device_prefs_clamp_beep_volume(int v) {
    if (v < (int)DEVICE_PREFS_BEEP_MIN) return DEVICE_PREFS_BEEP_MIN;
    if (v > (int)DEVICE_PREFS_BEEP_MAX) return DEVICE_PREFS_BEEP_MAX;
    return (uint8_t)v;
}

void device_prefs_clamp(device_prefs_t* p) {
    if (!p) return;
    p->brightness = device_prefs_clamp_brightness(p->brightness);
    p->beep_volume = device_prefs_clamp_beep_volume(p->beep_volume);
}

uint8_t device_prefs_beep_dac(uint8_t pct) {
    pct = device_prefs_clamp_beep_volume(pct);
    if (pct == 0) return 0;
    return (uint8_t)(((unsigned)DEVICE_PREFS_BEEP_DAC_MAX * pct + 50u) / 100u);
}

static bool parse_int_field(const char* json, const char* key, int* out) {
    char pat[40];
    int n = snprintf(pat, sizeof(pat), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(pat)) return false;
    const char* p = strstr(json, pat);
    if (!p) return false;
    p += (size_t)n;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return false;
    *out = (int)strtol(p, nullptr, 10);
    return true;
}

bool device_prefs_parse(const char* json, size_t len, device_prefs_t* out) {
    if (!out) return false;
    device_prefs_defaults(out);
    if (!json || len == 0) return false;

    char buf[192];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, json, len);
    buf[len] = '\0';

    const char* p = buf;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '{') return false;

    int v;
    if (parse_int_field(buf, "brightness", &v)) {
        out->brightness = device_prefs_clamp_brightness(v);
    }
    if (parse_int_field(buf, "beep_volume", &v)) {
        out->beep_volume = device_prefs_clamp_beep_volume(v);
    }
    return true;
}

size_t device_prefs_format(const device_prefs_t* p, char* out, size_t cap) {
    if (!p || !out || cap == 0) return 0;
    device_prefs_t c = *p;
    device_prefs_clamp(&c);
    int n = snprintf(out, cap, "{\"brightness\":%u,\"beep_volume\":%u}",
                     (unsigned)c.brightness, (unsigned)c.beep_volume);
    if (n <= 0 || (size_t)n >= cap) {
        if (cap) out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}
