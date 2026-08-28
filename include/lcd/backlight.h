#pragma once

#include <stdint.h>

// PWM backlight on GPIO 23 (active high). `percent` is clamped to 10-100 so
// the panel cannot be turned fully off from software. Call after
// st7701_lcd::begin().
void lcd_set_brightness(uint8_t percent);
