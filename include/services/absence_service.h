#pragma once
#include "app/roster.h"

#include <stddef.h>

// Absences (FREQ) for each student, limit is set to 25% of total classes.
int get_absence_service(const class_rec_t* class_rec, const char* student_id, int* limit);

// "Faltas: N/L" or "Faltas: N" when the class has no total_classes.
void format_absence_line(char* out, size_t cap, const class_rec_t* cls, const char* student_id);
