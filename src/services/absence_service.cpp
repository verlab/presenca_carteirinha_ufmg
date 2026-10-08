#include "services/absence_service.h"
#include <stdio.h>
#include <string.h>
#include "app/roster.h"
#include "services/export_service.h"
#include "services/roster_service.h"

int get_absence_service(const class_rec_t* cls, const char* student_id, int* limit) {
   if (limit) {
    if (cls && cls->total_classes > 0) {
        *limit = (cls->total_classes * 25) / 100;
    } else {
        *limit = 0;
    }
   }

    if (!cls || !student_id) return 0;

    int freq[ROSTER_MAX_CLASS_STUDENTS];
    if (!export_compute_freq(cls, freq)) return 0;

    for (int j = 0; j < cls->roster_count; j++) {
        const student_t* st = roster_student_at(cls->roster[j]);
        if (st && strcmp(st->id, student_id) == 0) return freq[j];
    }
    return 0;
}

void format_absence_line(char* out, size_t cap, const class_rec_t* cls, const char* student_id) {
    if (!out || cap == 0) return;
    int limit = 0;
    int freq = get_absence_service(cls, student_id, &limit);
    if (limit > 0) {
        snprintf(out, cap, "Faltas: %d/%d", freq, limit);
    } else {
        snprintf(out, cap, "Faltas: %d", freq);
    }
}
