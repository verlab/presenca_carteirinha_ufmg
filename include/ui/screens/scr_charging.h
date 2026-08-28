#pragma once

#include "ui/screen.h"

// Charging mode: signs the professor out, shows a brief hint, then turns the
// backlight off so most of the USB input goes into the battery instead of the
// display (the board's IP5306 cannot power the system down while a charger is
// plugged in - see docs/hardware/schematic/1_PWR.png, the KEY-pin circuit).
// Touching the screen or tapping a card wakes it back to the idle gate.
// Reachable from the Admin panel.
extern const screen_t scr_charging;
