#include "ui/screens/scr_charging.h"

#include "app/session.h"
#include "lcd/st7701_lcd.h"
#include "ui/screen_manager.h"
#include "ui/theme/theme.h"
#include "ui/ui.h"
#include "ui/ui_state.h"

// How long the hint stays readable before the backlight goes dark. Long enough
// to read "touch the screen to wake", short enough that the point of the mode
// (the display not burning power) starts quickly.
static constexpr uint32_t HINT_MS = 2500;

static lv_timer_t* s_off_timer = nullptr;

static void backlight_off_cb(lv_timer_t*) {
    s_off_timer = nullptr;  // one-shot: LVGL deletes it after this returns
    st7701_backlight(false);
}

// on_hide restores the backlight, so waking is just leaving.
static void wake(void) { scr_mgr_show(SCREEN_IDLE, nullptr); }

static void wake_press_cb(lv_event_t*) { wake(); }
static void wake_card_cb(const char*) { wake(); }

static lv_obj_t* create(void) {
    lv_obj_t* root = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(root, lv_color_hex(THEME_DARK_BG_DEEP), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(root, 12, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    // PRESSED, not CLICKED: waking must not depend on a clean tap-and-release
    // from someone touching a pitch-black screen.
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, wake_press_cb, LV_EVENT_PRESSED, nullptr);

    ui_make_label(root, LV_SYMBOL_CHARGE, THEME_DARK_ACCENT, &lv_font_montserrat_32);
    ui_make_label(root, "Charging", THEME_DARK_TEXT, &lv_font_montserrat_20);
    ui_make_label(root, "Screen turns off to charge faster", THEME_DARK_MUTED,
                  &lv_font_montserrat_14);
    ui_make_label(root, "Touch the screen to wake", THEME_DARK_MUTED, &lv_font_montserrat_14);

    return root;
}

static void on_show(void*) {
    // Entering charging mode is walking away from the device: end the session
    // so waking lands on the locked idle gate, not an open admin panel.
    session_set(nullptr);
    ui_state_set_user(nullptr);

    // A card tap wakes too (no login - the capture is consumed by waking).
    ui_set_card_capture(wake_card_cb);

    s_off_timer = lv_timer_create(backlight_off_cb, HINT_MS, nullptr);
    lv_timer_set_repeat_count(s_off_timer, 1);
}

static void on_hide(void) {
    if (s_off_timer) {
        lv_timer_delete(s_off_timer);
        s_off_timer = nullptr;
    }
    // Disarm the wake capture (a touch-wake would otherwise leave it armed to
    // swallow the next card tap on the idle gate), then light the panel back
    // up - unconditionally, so no path out of this screen leaves it dark.
    ui_set_card_capture(nullptr);
    st7701_backlight(true);
}

const screen_t scr_charging = {
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
