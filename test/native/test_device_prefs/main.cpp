// Device prefs: clamp, JSON parse/format, and the /settings.json round-trip
// on the in-memory SD card.

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "app/device_prefs.h"
#include "mock_sd.h"
#include "storage/device_prefs_store.h"

void setUp(void) { mocksd_reset(); }
void tearDown(void) {}

static const char* PATH = "/settings.json";

static void assert_file(const char* expected) {
    char buf[128] = {0};
    size_t n = mocksd_read_file(PATH, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

// ---- clamp / defaults ------------------------------------------------------

static void test_defaults(void) {
    device_prefs_t p = {0, 0, false};
    device_prefs_defaults(&p);
    TEST_ASSERT_EQUAL_UINT8(100, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(90, p.beep_volume);
    TEST_ASSERT_FALSE(p.no_rtc);
}

static void test_brightness_floor_keeps_the_panel_visible(void) {
    TEST_ASSERT_EQUAL_UINT8(10, device_prefs_clamp_brightness(0));
    TEST_ASSERT_EQUAL_UINT8(10, device_prefs_clamp_brightness(-20));
    TEST_ASSERT_EQUAL_UINT8(10, device_prefs_clamp_brightness(9));
}

static void test_brightness_ceiling(void) {
    TEST_ASSERT_EQUAL_UINT8(100, device_prefs_clamp_brightness(100));
    TEST_ASSERT_EQUAL_UINT8(100, device_prefs_clamp_brightness(255));
    TEST_ASSERT_EQUAL_UINT8(50, device_prefs_clamp_brightness(50));
}

static void test_beep_mute_and_ceiling(void) {
    TEST_ASSERT_EQUAL_UINT8(0, device_prefs_clamp_beep_volume(0));
    TEST_ASSERT_EQUAL_UINT8(0, device_prefs_clamp_beep_volume(-1));
    TEST_ASSERT_EQUAL_UINT8(100, device_prefs_clamp_beep_volume(100));
    TEST_ASSERT_EQUAL_UINT8(100, device_prefs_clamp_beep_volume(200));
}

static void test_beep_dac_mute_and_full(void) {
    TEST_ASSERT_EQUAL_UINT8(0, device_prefs_beep_dac(0));
    TEST_ASSERT_EQUAL_UINT8(0xBF, device_prefs_beep_dac(100));
    TEST_ASSERT_TRUE(device_prefs_beep_dac(1) > 0);
}

// ---- JSON ------------------------------------------------------------------

static void test_parse_both_keys(void) {
    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_parse("{\"brightness\":40,\"beep_volume\":25}",
                                       strlen("{\"brightness\":40,\"beep_volume\":25}"), &p));
    TEST_ASSERT_EQUAL_UINT8(40, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(25, p.beep_volume);
}

static void test_parse_missing_keys_keep_defaults(void) {
    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_parse("{}", 2, &p));
    TEST_ASSERT_EQUAL_UINT8(100, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(90, p.beep_volume);
    TEST_ASSERT_FALSE(p.no_rtc);
}

static void test_parse_clamps_out_of_range(void) {
    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_parse("{\"brightness\":0,\"beep_volume\":999}",
                                       strlen("{\"brightness\":0,\"beep_volume\":999}"), &p));
    TEST_ASSERT_EQUAL_UINT8(10, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(100, p.beep_volume);
}

static void test_parse_rejects_garbage(void) {
    device_prefs_t p = {1, 2};
    TEST_ASSERT_FALSE(device_prefs_parse("not json", 8, &p));
    TEST_ASSERT_EQUAL_UINT8(100, p.brightness);  // still filled with defaults
    TEST_ASSERT_EQUAL_UINT8(90, p.beep_volume);
}

static void test_format_roundtrip(void) {
    device_prefs_t in = {35, 12, true};
    char buf[80];
    size_t n = device_prefs_format(&in, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"brightness\":35,\"beep_volume\":12,\"no_rtc\":true}", buf);
    TEST_ASSERT_EQUAL(strlen(buf), n);

    device_prefs_t out;
    TEST_ASSERT_TRUE(device_prefs_parse(buf, n, &out));
    TEST_ASSERT_EQUAL_UINT8(35, out.brightness);
    TEST_ASSERT_EQUAL_UINT8(12, out.beep_volume);
    TEST_ASSERT_TRUE(out.no_rtc);
}

static void test_parse_no_rtc(void) {
    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_parse("{\"no_rtc\":true}", strlen("{\"no_rtc\":true}"), &p));
    TEST_ASSERT_TRUE(p.no_rtc);
    TEST_ASSERT_TRUE(device_prefs_parse("{\"no_rtc\":false}", strlen("{\"no_rtc\":false}"), &p));
    TEST_ASSERT_FALSE(p.no_rtc);
}

static void test_timed_threshold_zero_without_clock(void) {
    TEST_ASSERT_EQUAL_INT(0, device_prefs_timed_threshold(45, false));
    TEST_ASSERT_EQUAL_INT(45, device_prefs_timed_threshold(45, true));
    TEST_ASSERT_EQUAL_INT(0, device_prefs_timed_threshold(-3, true));
}

static void test_use_timed_clock(void) {
    device_prefs_t p;
    device_prefs_defaults(&p);
    TEST_ASSERT_TRUE(device_prefs_use_timed_clock(&p, true, true));
    TEST_ASSERT_FALSE(device_prefs_use_timed_clock(&p, false, true));
    TEST_ASSERT_FALSE(device_prefs_use_timed_clock(&p, true, false));
    p.no_rtc = true;
    TEST_ASSERT_FALSE(device_prefs_use_timed_clock(&p, true, true));
}

// ---- store -----------------------------------------------------------------

static void test_path_is_at_the_card_root(void) {
    TEST_ASSERT_EQUAL_STRING(PATH, device_prefs_store_path());
}

static void test_load_missing_file_returns_defaults(void) {
    device_prefs_t p = {1, 2};
    TEST_ASSERT_TRUE(device_prefs_store_load(&p));
    TEST_ASSERT_EQUAL_UINT8(100, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(90, p.beep_volume);
    TEST_ASSERT_FALSE(mocksd_exists(PATH));
}

static void test_save_then_load(void) {
    device_prefs_t in = {40, 15, true};
    TEST_ASSERT_TRUE(device_prefs_store_save(&in));
    assert_file("{\"brightness\":40,\"beep_volume\":15,\"no_rtc\":true}");

    device_prefs_t out;
    TEST_ASSERT_TRUE(device_prefs_store_load(&out));
    TEST_ASSERT_EQUAL_UINT8(40, out.brightness);
    TEST_ASSERT_EQUAL_UINT8(15, out.beep_volume);
    TEST_ASSERT_TRUE(out.no_rtc);
}

static void test_load_clamps_a_hand_edited_file(void) {
    mocksd_add_file(PATH, "{\"brightness\":0,\"beep_volume\":200}");
    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_store_load(&p));
    TEST_ASSERT_EQUAL_UINT8(10, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(100, p.beep_volume);
}

static void test_load_recovers_an_interrupted_write(void) {
    mocksd_add_file("/settings.json.bak", "{\"brightness\":55,\"beep_volume\":30}");
    TEST_ASSERT_FALSE(mocksd_exists(PATH));

    device_prefs_t p;
    TEST_ASSERT_TRUE(device_prefs_store_load(&p));
    TEST_ASSERT_EQUAL_UINT8(55, p.brightness);
    TEST_ASSERT_EQUAL_UINT8(30, p.beep_volume);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults);
    RUN_TEST(test_brightness_floor_keeps_the_panel_visible);
    RUN_TEST(test_brightness_ceiling);
    RUN_TEST(test_beep_mute_and_ceiling);
    RUN_TEST(test_beep_dac_mute_and_full);
    RUN_TEST(test_parse_both_keys);
    RUN_TEST(test_parse_missing_keys_keep_defaults);
    RUN_TEST(test_parse_clamps_out_of_range);
    RUN_TEST(test_parse_rejects_garbage);
    RUN_TEST(test_format_roundtrip);
    RUN_TEST(test_parse_no_rtc);
    RUN_TEST(test_timed_threshold_zero_without_clock);
    RUN_TEST(test_use_timed_clock);
    RUN_TEST(test_path_is_at_the_card_root);
    RUN_TEST(test_load_missing_file_returns_defaults);
    RUN_TEST(test_save_then_load);
    RUN_TEST(test_load_clamps_a_hand_edited_file);
    RUN_TEST(test_load_recovers_an_interrupted_write);
    return UNITY_END();
}
