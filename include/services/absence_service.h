#pragma once
#include "app/roster.h"

// Absences (FREQ) for each student, limit is set to 25% of total classes.
int get_absence_service(const class_rec_t* class_rec, const char* student_id, int* limit);
