// attendance_store against the in-memory SD card: append-only JSONL sessions,
// last-write-wins fold, persistence across reopen, date listing.
//
// Ordering: the first test establishes the SD mount for the binary.

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "mock_sd.h"
#include "storage/attendance_store.h"

// mocksd_reset() swaps the whole card out from under the module, which no real
// device does; start every test with a cold history cache so one test's counts
// can never answer another's.
void setUp(void) { attendance_history_cache_clear(); }
void tearDown(void) { attendance_close(); }

static const char* DIR = "CS101-M1";

static void read_file(const char* path, char* buf, size_t cap) {
    size_t n = mocksd_read_file(path, buf, cap - 1);
    buf[n] = '\0';
}

static void test_fresh_session_is_empty(void) {
    mocksd_reset();
    TEST_ASSERT_TRUE(attendance_open(DIR, "2026-07-16", 0));
    TEST_ASSERT_TRUE(attendance_is_open());
    TEST_ASSERT_EQUAL_STRING("2026-07-16", attendance_date());
    TEST_ASSERT_EQUAL_STRING(DIR, attendance_dir());
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
}

static void test_mark_present_appends_and_updates(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-16", 0);
    TEST_ASSERT_TRUE(attendance_set("2023-0142", true));
    TEST_ASSERT_TRUE(attendance_set("2023-0187", true));
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
    TEST_ASSERT_TRUE(attendance_is_present("2023-0187"));
    TEST_ASSERT_FALSE(attendance_is_present("9999-9999"));
    TEST_ASSERT_EQUAL_INT(2, attendance_present_count());

    // Two JSONL lines were appended.
    char buf[512];
    read_file("/classes/CS101-M1/attendance/2026-07-16.jsonl", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"2023-0142\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"present\":true"));
}

static void test_toggle_off_is_last_write_wins(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-16", 0);
    attendance_set("2023-0142", true);
    attendance_set("2023-0142", false);  // changed their mind
    TEST_ASSERT_FALSE(attendance_is_present("2023-0142"));
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
    // Both lines are on disk (append-only, not rewritten).
    char buf[512];
    read_file("/classes/CS101-M1/attendance/2026-07-16.jsonl", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"present\":true"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"present\":false"));
}

static void test_reopen_folds_persisted_state(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-16", 0);
    attendance_set("2023-0142", true);
    attendance_set("2023-0187", true);
    attendance_set("2023-0187", false);
    attendance_close();
    TEST_ASSERT_FALSE(attendance_is_open());

    // Reopening the same date reconstructs the present set from the log.
    TEST_ASSERT_TRUE(attendance_open(DIR, "2026-07-16", 0));
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
    TEST_ASSERT_FALSE(attendance_is_present("2023-0187"));
    TEST_ASSERT_EQUAL_INT(1, attendance_present_count());
}

static void test_sessions_are_separate_files(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-14", 0);
    attendance_set("2023-0142", true);
    attendance_open(DIR, "2026-07-16", 0);  // different day, fresh
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
    attendance_set("2023-0187", true);

    TEST_ASSERT_TRUE(mocksd_exists("/classes/CS101-M1/attendance/2026-07-14.jsonl"));
    TEST_ASSERT_TRUE(mocksd_exists("/classes/CS101-M1/attendance/2026-07-16.jsonl"));
}

static void test_list_dates_newest_first(void) {
    mocksd_reset();
    // Create three sessions out of order.
    attendance_open(DIR, "2026-07-14", 0);
    attendance_set("a", true);
    attendance_open(DIR, "2026-07-20", 0);
    attendance_set("a", true);
    attendance_open(DIR, "2026-07-09", 0);
    attendance_set("a", true);
    attendance_close();

    char dates[8][12];
    int n = attendance_list_dates(DIR, dates, 8);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_STRING("2026-07-20", dates[0]);
    TEST_ASSERT_EQUAL_STRING("2026-07-14", dates[1]);
    TEST_ASSERT_EQUAL_STRING("2026-07-09", dates[2]);

    TEST_ASSERT_EQUAL_INT(0, attendance_list_dates("NO-SUCH", dates, 8));
}

static void test_present_for_does_not_disturb_open_session(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-14", 0);
    attendance_set("a", true);
    attendance_set("b", true);
    attendance_close();

    attendance_open(DIR, "2026-07-16", 0);  // current session
    attendance_set("c", true);

    // Querying a past date must not change the open session.
    TEST_ASSERT_EQUAL_INT(2, attendance_present_for(DIR, "2026-07-14"));
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2099-01-01"));  // missing file
    TEST_ASSERT_EQUAL_STRING("2026-07-16", attendance_date());
    TEST_ASSERT_EQUAL_INT(1, attendance_present_count());
    TEST_ASSERT_TRUE(attendance_is_present("c"));
}

static void test_clear_removes_all_sessions(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-14", 0);
    attendance_set("a", true);
    attendance_open(DIR, "2026-07-16", 0);
    attendance_set("b", true);
    attendance_close();
    TEST_ASSERT_TRUE(mocksd_exists("/classes/CS101-M1/attendance/2026-07-14.jsonl"));

    int failed = -1;
    int removed = attendance_clear(DIR, &failed, nullptr, nullptr);
    TEST_ASSERT_EQUAL_INT(2, removed);
    TEST_ASSERT_EQUAL_INT(0, failed);  // nothing left behind
    TEST_ASSERT_FALSE(mocksd_exists("/classes/CS101-M1/attendance/2026-07-14.jsonl"));
    TEST_ASSERT_FALSE(mocksd_exists("/classes/CS101-M1/attendance/2026-07-16.jsonl"));
    TEST_ASSERT_FALSE(attendance_is_open());

    char dates[8][12];
    TEST_ASSERT_EQUAL_INT(0, attendance_list_dates(DIR, dates, 8));
    TEST_ASSERT_EQUAL_INT(0, attendance_clear(DIR, nullptr, nullptr, nullptr));  // idempotent; NULL out-param ok
}

// --- timed (double-tap) attendance ------------------------------------------

static const long long MIN_US = 60000000LL;  // one minute in microseconds

static void test_timed_present_when_over_threshold(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);

    // Tap in at t=0: in progress, not yet counted, whole threshold still to wait.
    att_state_t s = attendance_tap("2023-0142", 0, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_EQUAL_INT(45, s.remaining);
    TEST_ASSERT_FALSE(attendance_is_present("2023-0142"));
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());

    // A poll partway through reports the running minutes and the countdown.
    s = attendance_tap_state("2023-0142", 30 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_EQUAL_INT(30, s.minutes);
    TEST_ASSERT_EQUAL_INT(15, s.remaining);

    // Confirming tap at t=52 min: present, minutes recorded and persisted.
    s = attendance_tap("2023-0142", 52 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(52, s.minutes);
    TEST_ASSERT_EQUAL_INT(0, s.remaining);
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
    TEST_ASSERT_EQUAL_INT(1, attendance_present_count());

    char buf[256];
    read_file("/classes/CS101-M1/attendance/2026-07-20.jsonl", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"min\":52"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"present\":true"));
}

// A tap before the threshold does not register a presence: the arrival stands
// (refreshed on disk as a checkpoint) so the student can come back later.
static void test_timed_early_tap_does_not_register(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    att_state_t s = attendance_tap("2023-0142", 12 * MIN_US, 45);  // 33 min too soon
    TEST_ASSERT_EQUAL_INT(ATT_TOO_EARLY, s.status);
    TEST_ASSERT_EQUAL_INT(12, s.minutes);
    TEST_ASSERT_EQUAL_INT(33, s.remaining);
    TEST_ASSERT_FALSE(attendance_is_present("2023-0142"));
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
    // On disk: only arrival checkpoints so far, never a registered presence.
    char buf[512];
    read_file("/classes/CS101-M1/attendance/2026-07-20.jsonl", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"tapin\":true"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"min\":12"));
    TEST_ASSERT_NULL(strstr(buf, "\"present\":true"));

    // The original arrival is still the reference: waiting it out registers.
    s = attendance_tap("2023-0142", 50 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(50, s.minutes);
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
}

// The countdown rounds up, so a student mid-minute is never told "0 min left"
// while the threshold is not actually met.
static void test_timed_remaining_rounds_up(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    att_state_t s = attendance_tap("2023-0142", 44 * MIN_US + MIN_US / 2, 45);
    TEST_ASSERT_EQUAL_INT(ATT_TOO_EARLY, s.status);
    TEST_ASSERT_EQUAL_INT(44, s.minutes);
    TEST_ASSERT_EQUAL_INT(1, s.remaining);
}

// Once registered, further taps are ignored — reported, but never re-recorded.
static void test_timed_extra_tap_after_present_is_ignored(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_tap("2023-0142", 50 * MIN_US, 45);  // registered, 50 min

    char before[256];
    read_file("/classes/CS101-M1/attendance/2026-07-20.jsonl", before, sizeof(before));

    att_state_t s = attendance_tap("2023-0142", 80 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_ALREADY_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(50, s.minutes);  // still the registered duration
    TEST_ASSERT_EQUAL_INT(1, attendance_present_count());

    char after[256];
    read_file("/classes/CS101-M1/attendance/2026-07-20.jsonl", after, sizeof(after));
    TEST_ASSERT_EQUAL_STRING(before, after);  // no extra line
}

// Never confirming keeps the student out of the present set — but the arrival
// itself is persisted, so a reopen (as after a power failure) resumes the wait
// from the last checkpoint instead of voiding it.
static void test_timed_no_confirm_is_absent_but_resumes(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);  // in, never confirmed
    // Still in progress live...
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS,
                          attendance_tap_state("2023-0142", 90 * MIN_US, 45).status);
    // ...but not present.
    TEST_ASSERT_FALSE(attendance_is_present("2023-0142"));
    // Reopen with a rebooted clock: still not present, in progress again with
    // the checkpointed credit (0 min here — no checkpoint ever ran), and the
    // full threshold ahead. Uncheckpointed time is deliberately not credited.
    attendance_open(DIR, "2026-07-20", 7 * MIN_US);
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
    att_state_t s = attendance_tap_state("2023-0142", 7 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_EQUAL_INT(0, s.minutes);
    TEST_ASSERT_EQUAL_INT(45, s.remaining);
}

static void test_manual_override_clears_in_progress(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);  // in progress
    TEST_ASSERT_TRUE(attendance_set("2023-0142", true));  // professor forces present
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
    // The in-progress record was cleared and the student counts as present, so a
    // later tap is just ignored.
    att_state_t s = attendance_tap("2023-0142", 10 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_ALREADY_PRESENT, s.status);
}

static void test_timed_present_survives_reopen(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_tap("2023-0142", 50 * MIN_US, 45);  // present, 50 min
    attendance_open(DIR, "2026-07-20", 0);         // reopen folds the file
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));
    att_state_t s = attendance_tap_state("2023-0142", 0, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(50, s.minutes);
}

// Power-failure resume: the periodic checkpoint persists progress, and a reopen
// credits exactly the checkpointed minutes — the wait continues where it left
// off instead of restarting from zero (or disappearing).
static void test_timed_checkpoint_resumes_after_reopen(void) {
    mocksd_reset();
    TEST_ASSERT_EQUAL_INT(0, attendance_checkpoint(0));  // no session: no-op
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_tap("2023-0187", 0, 45);

    // One line per arrival with progress; a re-run at the same minute writes
    // nothing (no line per timer tick without movement).
    TEST_ASSERT_EQUAL_INT(2, attendance_checkpoint(30 * MIN_US));
    TEST_ASSERT_EQUAL_INT(0, attendance_checkpoint(30 * MIN_US));

    // "Power failure": reopen with a fresh (rebooted, smaller) clock. The
    // student resumes in progress with the 30 checkpointed minutes credited.
    attendance_open(DIR, "2026-07-20", 5 * MIN_US);
    att_state_t s = attendance_tap_state("2023-0142", 5 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_EQUAL_INT(30, s.minutes);
    TEST_ASSERT_EQUAL_INT(15, s.remaining);

    // Waiting out the remaining 15 minutes registers with the full total.
    s = attendance_tap("2023-0142", 20 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(45, s.minutes);
    TEST_ASSERT_TRUE(s.saved);
}

// A too-early tap refreshes the on-disk checkpoint, so its progress survives a
// reopen even without the periodic checkpoint running.
static void test_timed_early_tap_checkpoint_survives(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_tap("2023-0142", 20 * MIN_US, 45);  // too early; checkpoints 20
    attendance_open(DIR, "2026-07-20", 0);         // reboot
    att_state_t s = attendance_tap_state("2023-0142", 0, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_EQUAL_INT(20, s.minutes);
    TEST_ASSERT_EQUAL_INT(25, s.remaining);
}

// A manual override lands after the arrival in the JSONL, so it supersedes the
// checkpoint on disk exactly as it does in RAM.
static void test_manual_override_supersedes_checkpoint_on_disk(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_checkpoint(10 * MIN_US);
    attendance_set("2023-0142", false);     // professor: not here
    attendance_open(DIR, "2026-07-20", 0);  // reboot
    TEST_ASSERT_EQUAL_INT(ATT_ABSENT, attendance_tap_state("2023-0142", 0, 45).status);
    TEST_ASSERT_EQUAL_INT(0, attendance_present_count());
}

// Arrival checkpoints never count as presence in the history fold.
static void test_checkpoints_do_not_count_in_history(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    attendance_tap("2023-0142", 0, 45);
    attendance_checkpoint(30 * MIN_US);
    attendance_close();
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2026-07-20"));
}

// --- history count cache ----------------------------------------------------
//
// attendance_present_for() memoises, so these tests mutate the file behind its
// back to prove whether an answer came from the cache or from disk.

static void test_present_for_is_memoised(void) {
    mocksd_reset();
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-20.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n");
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));

    // Rewrite the file with two present students. A cached answer still says 1.
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-20.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n{\"id\":\"b\",\"present\":true}\n");
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));

    // Clearing the cache re-reads it.
    attendance_history_cache_clear();
    TEST_ASSERT_EQUAL_INT(2, attendance_present_for(DIR, "2026-07-20"));
}

// A tap through this module must invalidate that day, or the history view would
// keep showing the pre-tap count.
static void test_marking_present_invalidates_that_date(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2026-07-20"));  // caches 0
    attendance_set("2023-0142", true);
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
    attendance_set("2023-0187", true);
    TEST_ASSERT_EQUAL_INT(2, attendance_present_for(DIR, "2026-07-20"));
    attendance_set("2023-0142", false);
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
}

static void test_timed_tap_invalidates_that_date(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-20", 0);
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2026-07-20"));  // caches 0
    attendance_tap("2023-0142", 0, 45);  // arrival checkpoint: still 0 present
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2026-07-20"));
    attendance_tap("2023-0142", 50 * MIN_US, 45);       // confirms: appends
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
}

// Only the written date is dropped; other days stay cached.
static void test_other_dates_stay_cached(void) {
    mocksd_reset();
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-19.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n");
    attendance_open(DIR, "2026-07-20", 0);
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-19"));

    // Change the OTHER day on disk, then write to the open one.
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-19.jsonl", "");
    attendance_set("2023-0142", true);

    // 07-19 is still served from cache; 07-20 was re-read.
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-19"));
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
}

// The cache holds one class; asking about another must not return its counts.
static void test_switching_class_does_not_leak_counts(void) {
    mocksd_reset();
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-20.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n");
    mocksd_add_file("/classes/MA110-F1/attendance/2026-07-20.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n{\"id\":\"b\",\"present\":true}\n");
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
    TEST_ASSERT_EQUAL_INT(2, attendance_present_for("MA110-F1", "2026-07-20"));
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
}

static void test_clear_drops_the_cache(void) {
    mocksd_reset();
    mocksd_add_file("/classes/CS101-M1/attendance/2026-07-20.jsonl",
                    "{\"id\":\"a\",\"present\":true}\n");
    TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, "2026-07-20"));
    attendance_clear(DIR, nullptr, nullptr, nullptr);
    TEST_ASSERT_EQUAL_INT(0, attendance_present_for(DIR, "2026-07-20"));
}

// More dates than the cache holds must still answer correctly — the extras are
// simply re-read every time.
static void test_more_dates_than_the_cache_holds(void) {
    mocksd_reset();
    char path[80], date[12];
    for (int i = 0; i < 30; i++) {
        snprintf(date, sizeof(date), "2026-06-%02d", i + 1);
        snprintf(path, sizeof(path), "/classes/CS101-M1/attendance/%s.jsonl", date);
        mocksd_add_file(path, "{\"id\":\"a\",\"present\":true}\n");
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < 30; i++) {
            snprintf(date, sizeof(date), "2026-06-%02d", i + 1);
            TEST_ASSERT_EQUAL_INT(1, attendance_present_for(DIR, date));
        }
    }
}

// --- write failures must be reported, not swallowed --------------------------
//
// On a full card the student is still marked present in RAM (the UI stays
// consistent), but nothing reaches the JSONL. If that comes back as success the
// kiosk shows a green "Checked in" and a whole session goes missing unnoticed —
// so both entry points have to say the write failed.

static void test_set_reports_write_failure(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-25", 0);
    mocksd_set_card_full(true);

    TEST_ASSERT_FALSE(attendance_set("2023-0142", true));
    // RAM still reflects the tap, so the roll call does not lie about what the
    // professor just did — only persistence failed.
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));

    mocksd_set_card_full(false);
    char buf[128];
    read_file("/classes/CS101-M1/attendance/2026-07-25.jsonl", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);  // nothing was persisted
}

static void test_timed_tap_reports_write_failure(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-26", 0);

    // The arrival record cannot reach the card: the tap still works for this
    // session, but the caller is told it would not survive a power failure.
    mocksd_set_card_full(true);
    att_state_t s = attendance_tap("2023-0142", 0, 45);
    TEST_ASSERT_EQUAL_INT(ATT_IN_PROGRESS, s.status);
    TEST_ASSERT_FALSE(s.saved);

    // The confirming tap appends the registration — it must report failure too.
    s = attendance_tap("2023-0142", 50 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_EQUAL_INT(50, s.minutes);
    TEST_ASSERT_FALSE(s.saved);
    TEST_ASSERT_TRUE(attendance_is_present("2023-0142"));

    mocksd_set_card_full(false);
    char buf[128];
    read_file("/classes/CS101-M1/attendance/2026-07-26.jsonl", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

// The happy path must keep reporting saved==true, or the warning UI would fire
// on every normal check-in.
static void test_timed_tap_reports_success_when_written(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-27", 0);
    attendance_tap("2023-0142", 0, 45);
    att_state_t s = attendance_tap("2023-0142", 50 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_PRESENT, s.status);
    TEST_ASSERT_TRUE(s.saved);

    // Non-writing states report saved==true so callers can branch on it alone.
    s = attendance_tap("2023-0142", 80 * MIN_US, 45);
    TEST_ASSERT_EQUAL_INT(ATT_ALREADY_PRESENT, s.status);
    TEST_ASSERT_TRUE(s.saved);
}

// The debug wipe deletes every session file of every class with the UI blocked,
// so it needs to report movement — an operator who reads the frozen screen as a
// crash and power-cycles it gets a half-wiped card.
static void test_clear_reports_one_item_per_file(void) {
    mocksd_reset();
    attendance_open(DIR, "2026-07-14", 0);
    attendance_set("a", true);
    attendance_open(DIR, "2026-07-16", 0);
    attendance_set("b", true);
    attendance_open(DIR, "2026-07-18", 0);
    attendance_set("c", true);
    attendance_close();

    struct rec_t {
        int calls, last_done, last_total;
    } rec = {0, 0, 0};
    auto cb = [](const progress_t* p, void* ctx) {
        rec_t* r = (rec_t*)ctx;
        r->calls++;
        r->last_done = p->done;
        r->last_total = p->total;
    };

    TEST_ASSERT_EQUAL_INT(3, attendance_clear(DIR, nullptr, cb, &rec));
    // The dates are listed before anything is deleted, so this one knows its
    // total up front and the bar can be determinate.
    TEST_ASSERT_EQUAL_INT(3, rec.calls);
    TEST_ASSERT_EQUAL_INT(3, rec.last_total);
    TEST_ASSERT_EQUAL_INT(3, rec.last_done);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_session_is_empty);
    RUN_TEST(test_mark_present_appends_and_updates);
    RUN_TEST(test_toggle_off_is_last_write_wins);
    RUN_TEST(test_reopen_folds_persisted_state);
    RUN_TEST(test_sessions_are_separate_files);
    RUN_TEST(test_list_dates_newest_first);
    RUN_TEST(test_present_for_does_not_disturb_open_session);
    RUN_TEST(test_clear_removes_all_sessions);
    RUN_TEST(test_timed_present_when_over_threshold);
    RUN_TEST(test_timed_early_tap_does_not_register);
    RUN_TEST(test_timed_remaining_rounds_up);
    RUN_TEST(test_timed_extra_tap_after_present_is_ignored);
    RUN_TEST(test_timed_no_confirm_is_absent_but_resumes);
    RUN_TEST(test_manual_override_clears_in_progress);
    RUN_TEST(test_timed_present_survives_reopen);
    RUN_TEST(test_timed_checkpoint_resumes_after_reopen);
    RUN_TEST(test_timed_early_tap_checkpoint_survives);
    RUN_TEST(test_manual_override_supersedes_checkpoint_on_disk);
    RUN_TEST(test_checkpoints_do_not_count_in_history);
    RUN_TEST(test_present_for_is_memoised);
    RUN_TEST(test_marking_present_invalidates_that_date);
    RUN_TEST(test_timed_tap_invalidates_that_date);
    RUN_TEST(test_other_dates_stay_cached);
    RUN_TEST(test_switching_class_does_not_leak_counts);
    RUN_TEST(test_clear_drops_the_cache);
    RUN_TEST(test_more_dates_than_the_cache_holds);
    RUN_TEST(test_set_reports_write_failure);
    RUN_TEST(test_timed_tap_reports_write_failure);
    RUN_TEST(test_timed_tap_reports_success_when_written);
    RUN_TEST(test_clear_reports_one_item_per_file);
    return UNITY_END();
}
