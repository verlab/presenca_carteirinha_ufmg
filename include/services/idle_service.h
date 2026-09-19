#pragma once
#include <stdint.h>

// Reduces the backlight after 1 min of no touch orcard activity, restoring the
// brightness on the next touch or card scan
void idle_service_init(void);

// Call whenever a card is scanned or the touch activity is detected
void activity_detected(void);

// Sets the brightness when necessary
void idle_setbrightness(uint8_t percent);
