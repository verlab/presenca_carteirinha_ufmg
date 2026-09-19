#pragma once 
#include "app/roster.h"

// Counts how many classes are planned for the semester: for each window in
// `windows`, sums how many times that weekday falls between the later of
// semester_start/class_start and semester_end. Invalid dates or
// an semester_end before the semester_start return 0.
int classes_persemester(const char* semester_start, const char* semester_end, const char* class_start, const session_t* windows, int window_count);

