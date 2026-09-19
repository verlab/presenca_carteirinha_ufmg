#include "services/idle_service.h"
#include <lvgl.h>
#include "lcd/st7701_lcd.h"
#include "lcd/backlight.h"
#include "storage/device_prefs_store.h"
#include "ui/screen_manager.h"
#include "ui/screens/scr_charging.h"

static constexpr uint32_t IDLE_TIMEOUT_MS = 60000;  // 1 min
static constexpr uint32_t SCREEN_BRIGHTNESS = 15;  // backlight percentage in low power mode

static uint8_t s_percent = 60; 
static bool s_idle = false; // whether we're currently in low power mode
static uint32_t s_lastcard = 0; // tick of the last card read
static lv_obj_t* s_wake = nullptr; // catches the first touch to exit low power mode

static void restore_brightness(void) {
    if(!s_idle) return;
    lcd_set_brightness(s_percent);
    s_idle = false;
    if(s_wake) {
        lv_obj_delete(s_wake);
        s_wake = nullptr;
    }
}

// if screen is touched, exit low-power mode.
static void screen_touched(lv_event_t*) {
    restore_brightness();
}

static void lowpower_mode(void) {
    if(s_idle) return;
    if(scr_mgr_current() == SCREEN_CHARGING) return; // nao entra no modo economico se estiver carregando

    lcd_set_brightness(SCREEN_BRIGHTNESS);
    s_idle = true;
    s_wake = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_wake);
    lv_obj_set_size(s_wake, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(s_wake, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_wake, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_wake, screen_touched, LV_EVENT_PRESSED, nullptr);

}

static void check(lv_timer_t*) {
    if (s_idle) return;

    // overall display inactivity time
    uint32_t touch = lv_disp_get_inactive_time(nullptr);
    uint32_t card = lv_tick_elaps(s_lastcard);

    // take whichever was more recent: last touch or last card read
    uint32_t min_time = touch < card ? touch : card;

    if (min_time >= IDLE_TIMEOUT_MS) {
        lowpower_mode();
    }
}

void idle_service_init(void) {
    device_prefs_t prefs;
    device_prefs_store_load(&prefs);
    s_percent = prefs.brightness;
    s_lastcard = lv_tick_get(); // starts the last card read clock
    lv_timer_create(check, 1000, nullptr); 
}

void activity_detected(void) {
    s_lastcard = lv_tick_get(); 
    restore_brightness();
}

void idle_setbrightness(uint8_t percent) {
    s_percent = percent;
    if(!s_idle) {
        lcd_set_brightness(s_percent);
    }
}