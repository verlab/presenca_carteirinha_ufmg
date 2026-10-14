#include "services/rtc_service.h"
#include <RTClib.h>
#include <Wire.h>
#include <stdio.h>
#include "esp32-hal-log.h"
#include "esp_timer.h"

static const char* TAG = "rtc";
static RTC_DS1307 s_rtc;
static bool s_present = false;
static bool s_configured = false;
static DateTime s_base_dt;
static int64_t s_base_us = 0;

static bool rtc_datetime_valid(const DateTime& dt) {
    return dt.isValid() && dt.year() >= 2024 && dt.year() <= 2099;
}


static void rtc_cache_set(const DateTime& dt) {
    s_base_dt = dt;
    s_base_us = esp_timer_get_time();
    s_configured = true;
}

static DateTime rtc_cached_now(void) {
    uint32_t elapsed = (uint32_t)((esp_timer_get_time() - s_base_us) / 1000000LL);
    return s_base_dt + TimeSpan(elapsed);
}

bool rtc_service_init(void) {
    s_present = s_rtc.begin(&Wire1);
    if (!s_present) {
        ESP_LOGW(TAG, "clock is not working");
        return false;
    }
    if (!s_rtc.isrunning()) {
        ESP_LOGW(TAG, "RTC date/time is not set");
        s_configured = false;
        return true;
    }

    DateTime now = s_rtc.now();
    s_configured = rtc_datetime_valid(now);
    if (!s_configured) {
        ESP_LOGW(TAG, "RTC returned invalid date");
        return true;
    }
    rtc_cache_set(now);
    ESP_LOGI(TAG, "RTC ok, current time: %04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(),
             now.day(), now.hour(), now.minute(), now.second());
    return true;
}

bool set_rtc(void) {
    return s_present && s_configured;
}

rtc_time_t rtc_now(void){
    rtc_time_t time = {};
    if(!set_rtc()) return time;
    DateTime now = rtc_cached_now();
    if (!rtc_datetime_valid(now)) {
        ESP_LOGW(TAG, "RTC returned invalid date during reading");
        s_configured = false;
        return time;
    }
    time.year = now.year();
    time.month = now.month();
    time.day = now.day();
    time.hour = now.hour();
    time.minute = now.minute();
    time.second = now.second();
    return time;
}

void rtc_today(char* out, size_t capacity){
    rtc_time_t time = rtc_now();
    snprintf(out, capacity, "%04d-%02d-%02d", time.year, time.month, time.day);
}

// check if the given date is valid 
bool rtc_adjust(int year, int month, int day, int hour, int minute, int second){
    DateTime dt((uint16_t)year, (uint8_t)month, (uint8_t)day, (uint8_t)hour, (uint8_t)minute,
                (uint8_t)second);
    if (!rtc_datetime_valid(dt)) {
        ESP_LOGW(TAG, "Invalid date: %04d-%02d-%02d %02d:%02d:%02d", year,
                 month, day, hour, minute, second);
        return false;
    }
    if (!s_present) {
        s_present = s_rtc.begin(&Wire1);  
        if (!s_present) {
            ESP_LOGW(TAG, "rtc_adjust: clock is not working");
            return false;
        }
    }
    s_rtc.adjust(dt);
    DateTime clock = s_rtc.now();
    s_configured = rtc_datetime_valid(clock);
    if (s_configured) rtc_cache_set(clock);
    return s_configured;
}

int rtc_weekday(void) {
    if (!set_rtc()) return -1;
    return rtc_cached_now().dayOfTheWeek();
}

uint32_t rtc_timestamp(void) {
    if (!set_rtc()) return 0;
    DateTime utc = rtc_cached_now() + TimeSpan(0, 3, 0, 0); // para o horario de brasília, que é utc-3 (no firebase eh utc-0)
    return utc.unixtime();
}