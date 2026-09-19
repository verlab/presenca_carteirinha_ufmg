#include "services/absence_service.h"
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
