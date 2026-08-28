#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app/progress.h"

// Per-class, per-day attendance in append-only JSONL files:
//
//   /classes/<dir>/attendance/YYYY-MM-DD.jsonl
//   {"id":"2023-0142","present":true}
//   {"id":"2023-0187","present":true}
//   {"id":"2023-0142","present":false}   <- last line for an id wins
//
// Append-only is deliberate: on a battery-less device that can be switched
// off mid-class, a failed append loses at most the last tap, never the file.
// The "current" presence is the fold of the file (last writer per id wins).
//
// One session is open at a time (all functions touch that session). LVGL
// thread only.

// Opens the session for class_dir + date, folding any existing file into
// memory (a missing file is a fresh, empty session). Replaces any open
// session. `now_us` is the current monotonic timestamp (esp_timer_get_time),
// used to resume any persisted timed arrivals — see the timed section below.
// Returns false only on a read error.
bool attendance_open(const char* class_dir, const char* date, long long now_us);

bool attendance_is_open(void);
const char* attendance_dir(void);   // "" when no session is open
const char* attendance_date(void);  // ""

bool attendance_is_present(const char* student_id);
int attendance_present_count(void);

// Records a presence change: appends one line and updates memory. Returns
// false on a write error (memory is still updated so the UI stays in sync).
bool attendance_set(const char* student_id, bool present);

// --- Timed (double-tap) attendance ------------------------------------------
//
// In timed mode a student taps twice: once on arrival, once to confirm after
// they have been in class long enough. The confirming tap only counts once
// (now - arrive) >= threshold; a tap before that is rejected without changing
// anything, so the student can simply come back later — the arrival still
// stands and the rejected tap reports the minutes still to wait. Once the
// presence is registered, further taps are ignored (ATT_ALREADY_PRESENT).
//
// Elapsed time comes from a monotonic clock (esp_timer_get_time), so it works
// without an RTC. The arrival itself is persisted: the first tap appends a
// checkpoint record to the same JSONL, and attendance_checkpoint() (plus any
// too-early tap) refreshes it with the minutes waited so far:
//   {"id":"2023-0142","present":false,"min":30,"tapin":true}
// On attendance_open() a trailing tapin record restores the arrival as
// in-progress, credited with its checkpointed minutes — so a power failure
// (or closing and reopening the session) resumes the wait instead of voiding
// it. Time past the last checkpoint is not counted (the monotonic clock does
// not survive a reboot), which errs on making the student wait, never on
// crediting time not proven. Registered results are written with "min" and no
// "tapin" flag, superseding the arrival on fold:
//   {"id":"2023-0142","present":true,"min":52}

typedef enum {
    ATT_ABSENT = 0,       // not tapped
    ATT_IN_PROGRESS,      // tapped in, waiting out the threshold
    ATT_PRESENT,          // this tap registered the presence
    ATT_TOO_EARLY,        // tapped before the threshold: nothing recorded, still waiting
    ATT_ALREADY_PRESENT,  // presence was already registered; this tap is ignored
} att_status_t;

typedef struct {
    att_status_t status;
    int minutes;    // elapsed so far (IN_PROGRESS/TOO_EARLY) or measured (PRESENT/ALREADY_PRESENT)
    int remaining;  // whole minutes left before a tap can register (IN_PROGRESS/TOO_EARLY; else 0)
    // False only when this call tried to append and the write failed — i.e. the
    // state is right in RAM but nothing reached the card. The UI MUST surface
    // that: a green "Presence registered" over a failed write is how a whole
    // session goes missing without anyone noticing. Two states write and can
    // report false: PRESENT (the registration itself) and IN_PROGRESS (the
    // arrival record — the tap works for this session but would not survive a
    // power failure). TOO_EARLY's checkpoint refresh is best-effort (an earlier
    // checkpoint still stands), so it reports true, as do the states that write
    // nothing (ALREADY_PRESENT, ABSENT).
    bool saved;
} att_state_t;

// Handles a timed tap for a student in the open session. The first tap records
// the arrival (in RAM and as a tapin record on disk). A later tap registers the
// presence (writes present+min) once the gap is >= threshold_min; before that
// it refreshes the on-disk checkpoint, returns ATT_TOO_EARLY with `remaining`
// set and does not register, and once registered it returns
// ATT_ALREADY_PRESENT. `now_us` is a monotonic timestamp (microseconds).
// Returns the resulting state.
att_state_t attendance_tap(const char* student_id, long long now_us, int threshold_min);

// The student's current timed state in the open session (for the roll call).
// A query, never a tap: it reports ATT_IN_PROGRESS (with the minutes elapsed so
// far and still to wait, per threshold_min) or ATT_PRESENT, never TOO_EARLY /
// ALREADY_PRESENT.
att_state_t attendance_tap_state(const char* student_id, long long now_us, int threshold_min);

// Appends a checkpoint record for every in-progress arrival whose whole-minute
// count advanced since its last one, so a power failure loses at most one
// checkpoint interval of waiting (the device runs this every five minutes, from
// ui/ui.cpp). The batch is a single file open/append/close, so the cost does
// not scale with class size. Call from the LVGL thread (SD writes live there);
// a no-op without an open session or pending arrivals. Returns records written.
int attendance_checkpoint(long long now_us);

// Ends the open session (clears memory; the log is already on disk).
void attendance_close(void);

// Past session dates for a class, newest first. Returns the count, filling up
// to `max` entries of `dates` (each buffer holds "YYYY-MM-DD" + NUL).
int attendance_list_dates(const char* class_dir, char dates[][12], int max);

// Present count for a specific past date without disturbing the open session
// (for the history summary). 0 if the file is missing.
//
// Memoised per (class_dir, date) — the class screens ask for every past date on
// each build, and each answer costs a whole-file fold. Writes through this
// module drop the affected entry, so the count is always current with respect to
// anything the device does itself.
int attendance_present_for(const char* class_dir, const char* date);

// Drops every memoised count. Only needed when a session file changed WITHOUT
// going through this module — in practice, the debug WiFi file editor.
void attendance_history_cache_clear(void);

// DEBUG: deletes every session log for a class (all YYYY-MM-DD.jsonl files) and
// closes any open session. Returns the number of files removed. When `out_failed`
// is non-NULL it receives the number of files that could NOT be deleted, so a
// partial wipe isn't reported as a clean success (each failure is also logged).
//
// `cb` (optional) reports one item per file. The dates are listed up front, so
// this one knows its total.
int attendance_clear(const char* class_dir, int* out_failed, progress_cb_t cb, void* ctx);
