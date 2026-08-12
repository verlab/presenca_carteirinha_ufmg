#pragma once

#include "app/device_prefs.h"

// Device-written `/settings.json`: screen brightness and beep volume. Not in
// a config.tar and not touched by import, so an operator's Admin choices
// survive a roster refresh. SD I/O; call on the LVGL thread.
//
// Load recovers an interrupted atomic write first. A missing file is not an
// error — `out` is filled with defaults and nothing is created until save.

bool device_prefs_store_load(device_prefs_t* out);
bool device_prefs_store_save(const device_prefs_t* p);

const char* device_prefs_store_path(void);
