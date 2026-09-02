#include "services/rtc_service.h"
#include "services/session_service.h"
#include "app/event_bus.h"
#include "app/roster.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "services/roster_service.h"
#include "storage/attendance_store.h"
#include "esp32-hal-log.h"
#include <string.h>

static constexpr uint32_t PERIOD_MS = 30000; 
static constexpr int MAX_DATE = 8;
static const char* TAG = "session";
  
static bool has_session_today(const class_rec_t* c, const char* today) {
    char dates[MAX_DATE][12];
    int n = attendance_list_dates(c->dir, dates, MAX_DATE);
    for (int i = 0; i < n; i++) {
        if (strcmp(dates[i], today) == 0) return true;
    }
    return false;
}

// checks all classes for a session that should be open now, and opens it if so
static void check_classes() {
    if(!set_rtc()){
        ESP_LOGW(TAG, "rtc not set, cannot check for sessions");
        return;
    }
    if(attendance_is_open()){
        ESP_LOGI(TAG, "Session already open (%s / %s)", attendance_dir(), attendance_date());
        return;
    }

    rtc_time_t time = rtc_now();
    int wd = rtc_weekday();
    int h_min = time.hour * 60 + time.minute;
    
    char today[11];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", time.year, time.month, time.day);

    ESP_LOGI(TAG, "checking: today=%s wd=%d hhmm=%d (classes=%d)", today, wd, h_min,
             roster_class_count());
    
    for(int i = 0; i < roster_class_count(); i++){
        const class_rec_t* c = roster_class_at(i);
        ESP_LOGI(TAG, "class %s: %d sessions", c->code, c->session_count);
        for(int j = 0; j < c->session_count; j++){
            const session_t* w = &c->sessions[j];
            if(w-> weekday != wd) continue;
            if(h_min < w->start_min || h_min > w->end_min) continue;
            bool has = has_session_today(c, today);
            if(has_session_today(c, today)) continue;

            if(attendance_open(c->dir, today, esp_timer_get_time())) {
                ESP_LOGI(TAG, "rtc opened session for class %s", c->code);
                app_event_t ev = {};
                ev.type = APP_EVENT_SESSION_OPENED;
                snprintf(ev.session.class_dir, sizeof(ev.session.class_dir), "%s", c->dir);
                event_bus_post(&ev);
            }
            return;
            
        }
    }

}

static void session_task(void*) {
    check_classes();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
        check_classes();
    }
}

bool session_service_init(void) {
    return xTaskCreate(session_task, "session", 3072, nullptr, 1, nullptr) == pdPASS;
}
