#include "../include/app/semester_calendar.h"
#include <stdio.h>

static long days(int y, int m, int d){
    if (m < 3) {
        y--;
    }

    long date;
    if (y >= 0) {
        date = y / 400;
    } else {
        date = (y - 399) / 400;
    }

    unsigned year_date = (unsigned)(y - date * 400);
    unsigned day = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned day_date = year_date * 365 + year_date / 4 - year_date / 100 + day;
    return date * 146097 + (long)day_date - 719468;
}

static int weekday(long days_since){
    long m = (days_since % 7 + 7 + 4) % 7;
    return (int)m;
}

static int weekday_occurrences(int weekdays, long d0, long d1){
     if (d1 < d0 || weekdays < 0 || weekdays > 6) return 0;
    int wd0 = weekday(d0);
    long offset = (weekdays - wd0 + 7) % 7;
    long first = d0 + offset;
    if (first > d1) return 0;
    return (int)((d1 - first) / 7 + 1);
}


int classes_persemester(const char* semester_start, const char* semester_end, const char* class_start, const session_t* windows, int window_count){
    if(!semester_start || !semester_end || !class_start || !windows || window_count <= 0){
        return 0;
    }
    int y0, m0, d0, y1, m1, d1;
    if (sscanf(semester_start, "%d-%d-%d", &y0, &m0, &d0) != 3) return 0;
    if (sscanf(semester_end, "%d-%d-%d", &y1, &m1, &d1) != 3) return 0;
    long start = days(y0, m0, d0);
    long end = days(y1, m1, d1);
    if (end < start) return 0;
    int total = 0;
    for (int i = 0; i < window_count; i++)
        total += weekday_occurrences(windows[i].weekday, start, end);
    return total;

}