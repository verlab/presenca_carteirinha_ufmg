#pragma once

// Native stand-in for the ESP high-resolution timer. Only esp_timer_get_time()
// is mocked, and only because code under test logs elapsed times with it; the
// tests assert on results, never on durations. Monotonic microseconds from the
// host clock is all that needs to be true.

#include <stdint.h>
#include <time.h>

static inline int64_t esp_timer_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
