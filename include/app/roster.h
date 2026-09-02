#pragma once

#include <stdint.h>

// Domain types for the SD-card student/class data (see roster_service.h for
// the on-card layout). Students live in one global registry keyed by
// university ID; classes reference them by registry index, so a student
// enrolled in several classes has exactly one record — and one RFID binding.

constexpr int ROSTER_MAX_STUDENTS = 600;   // global registry cap (see the RAM note below)
constexpr int ROSTER_MAX_CLASSES = 12;
// Per-class roster cap. Raising this is not free: it widens class_rec_t (which
// is held ROSTER_MAX_CLASSES times) AND attendance_store's present/tap-in sets,
// all in internal RAM. See the RAM note below for the measured cost.
constexpr int ROSTER_MAX_CLASS_STUDENTS = 250;
// A class can be co-taught. Mirrors CONFIG_MAX_TEACHERS (config_service.h) —
// a class can't reference more professors than the device can hold. The
// config-builder's schema-sync test asserts the two stay equal.
constexpr int ROSTER_MAX_CLASS_TEACHERS = 8;

// A class can have many sessions per week. 
constexpr int ROSTER_MAX_SESSIONS = 4;


// Face-verify countdown bounds (seconds) for a class's photo check-in.
constexpr int FACE_VERIFY_SECONDS_MIN = 3;
constexpr int FACE_VERIFY_SECONDS_MAX = 60;
constexpr int FACE_VERIFY_SECONDS_DEFAULT = 15;
// Timed attendance: minutes a student must wait before the confirming tap
// counts. class.json's optional "min_attendance_min".
constexpr int MIN_ATTENDANCE_MIN_DEFAULT = 45;
// NOTE: these caps are paid for in INTERNAL RAM, the scarce pool
// (docs/software/ARCHITECTURE.md §Memory budget). Measured with
// `riscv32-esp-elf-nm --size-sort -S`:
//
//   s_students[600]  55,200 B   600 * sizeof(student_t) (92 B)
//   s_classes[12]    62,016 B   12 * sizeof(class_rec_t) (5,168 B)
//   attendance_store 13,000 B   present + tap-in sets, sized by the class cap
//
// Each +1 on ROSTER_MAX_CLASS_STUDENTS costs 268 B: 216 B in s_classes (18 B per
// class x 12, and roster_turma is 16 of those 18) plus 52 B in attendance_store.
// The two raises so far, measured end to end:
//
//   100 -> 200   +26,808 B   static internal 134 KB -> 160 KB
//   200 -> 250   +13,408 B   static internal 160 KB -> 174 KB (177,900 B)
//
// That is the budget the face-detection model check (128 KB free / 64 KB largest
// block) and every sdmmc DMA transfer draw from, and 250 was taken WITHOUT first
// moving the arrays to PSRAM as BACKLOG.md advises. Before raising this again,
// take that lever (move s_students/s_classes to PSRAM at roster_service_start())
// rather than spending more of the pool.

typedef struct {
    char id[20];        // university ID, the stable key
    char name[48];
    char rfid_uid[24];  // empty = card not matched yet (bound on first tap)
} student_t;

typedef struct {
    int8_t weekday; // 0 = sunday, 1 = monday, ..., 6 = saturday
    int16_t start_min; 
    int16_t end_min;
} session_t;

typedef struct {
    char code[24];           // e.g. "CS101-M1"
    char dir[24];            // /classes/<dir>/ folder name (usually == code)
    char name[48];
    char schedule[40];
    // Professors who teach this class — links to config.json's teachers. A class
    // may be co-taught, so it shows for ANY of these (roster_class_matches_teacher).
    // From class.json's "teacher_emails" array; a legacy scalar "teacher_email"
    // is still accepted and lands here as a single entry.
    char teacher_emails[ROSTER_MAX_CLASS_TEACHERS][64];
    int8_t teacher_count;
    uint32_t color;
    // Photo check-in for this class (a per-class option; there is no device-wide
    // flag). When on, kiosk check-ins face-verify within face_verify_seconds and
    // save a photo. From class.json's optional "capture_photos" bool (default
    // false) + "face_verify_seconds" int (default 15, clamped).
    bool capture_photos;
    int16_t face_verify_seconds;
    // Timed (double-tap) attendance: students tap on arrival, then tap again to
    // register once >= min_attendance_min have passed (an earlier tap is
    // rejected, not recorded). From class.json's optional "timed_attendance"
    // bool + "min_attendance_min" int (default 45).
    bool timed_attendance;
    int16_t min_attendance_min;
    // Per-student class group ("turma") lives on each class.json roster entry
    // ({"id","turma"}). Parallel to roster[]: roster_turma[i] is the group of the
    // student at roster[i] IN THIS CLASS (empty when untagged). Attendance is
    // still by student index; turma is display/statistics only.
    
    session_t sessions[ROSTER_MAX_SESSIONS];
    int8_t session_count;

    int16_t roster[ROSTER_MAX_CLASS_STUDENTS];   // indexes into the registry
    char roster_turma[ROSTER_MAX_CLASS_STUDENTS][16];
    int roster_count;
} class_rec_t;
