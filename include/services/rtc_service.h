#pragma once

#include <stddef.h>
#include <stdint.h>

// Data structure representing broken-down RTC time components
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} rtc_time_t;

// Initializes the DS1307 RTC hardware module over Wire1 bus and caches initial timestamp.
// Returns true on success or if clock is present; false if hardware fails to respond.
bool rtc_service_init(void);

// Returns true if the RTC module was detected and successfully configured with valid time.
bool set_rtc(void);

// Retrieves the current date and time components computed via hardware cache and ESP timer offset.
rtc_time_t rtc_now(void);

// Formats the current cached RTC date into ISO 8601 string format (YYYY-MM-DD).
void rtc_today(char* out, size_t capacity);

// Adjusts hardware RTC time registers and resets the microsecond reference timer.
// Returns true if the new timestamp is valid and successfully written to hardware.
bool rtc_adjust(int year, int month, int day, int hour, int minute, int second);

// Returns the day of the week index (0 = Sunday, 6 = Saturday) or -1 if RTC is unconfigured.
int rtc_weekday(void);

uint32_t rtc_timestamp(void);
