// time.h - calendar time and processor time.
//
// The one header here where a **struct layout** has to be right rather than
// merely big enough, because a program declares its own 'struct tm' and hands
// the address to mktime or strftime. Get it short and the library writes past
// the end of it.
//
// C90 defines nine members. The Unix platforms add two more after them -
// tm_gmtoff and tm_zone - and the UCRT adds none, which is exactly the 56
// against 36 bytes measured on the two kinds of platform. Both are declared
// below, so a struct tm here is the same object the platform's own header
// describes.
//
// The two extra members are named rather than reserved because a program that
// reads tm_zone after localtime is doing something reasonable, and hiding the
// field would only make it declare its own struct.
//
// clock_t is the other thing that moves: eight bytes on Linux and macOS, four
// under the UCRT - and CLOCKS_PER_SEC is a million on the first two and a
// thousand on the third. A program dividing by it gets seconds on all three;
// a program assuming microseconds gets them on two.
#ifndef _CC1_TIME_H
#define _CC1_TIME_H

#include <stddef.h>

// Seconds since the epoch. Eight bytes on all three - the UCRT defaults
// time_t to 64 bits, which is why nothing here needs _USE_32BIT_TIME_T.
typedef long time_t;

#ifdef _WIN32
typedef int clock_t;
#define CLOCKS_PER_SEC 1000
#else
typedef long clock_t;
#define CLOCKS_PER_SEC 1000000
#endif

struct tm {
    int tm_sec;     // 0-60, the 60 being a leap second
    int tm_min;     // 0-59
    int tm_hour;    // 0-23
    int tm_mday;    // 1-31
    int tm_mon;     // 0-11, January being 0
    int tm_year;    // years since 1900, so 2026 is 126
    int tm_wday;    // 0-6, Sunday being 0
    int tm_yday;    // 0-365
    int tm_isdst;   // positive if daylight saving is in effect, 0 if not,
                    // negative if the answer is unknown - and mktime reads
                    // this one, so leaving it uninitialised is a real bug
#ifndef _WIN32
    // Not C90, and present because the platform's struct has them: a shorter
    // struct here would be written past by localtime.
    long tm_gmtoff;
    const char *tm_zone;
#endif
};

time_t time(time_t *t);
double difftime(time_t end, time_t start);
clock_t clock(void);

// Both return a pointer to a static object the library owns, so a second call
// overwrites the first. Copy what you need before calling again.
struct tm *localtime(const time_t *t);
struct tm *gmtime(const time_t *t);

// mktime also *normalises* what it is given - a tm_mday of 32 in January comes
// back as the 1st of February - which is the ordinary way to do date
// arithmetic in C90.
time_t mktime(struct tm *tm);

// Both write into a static buffer, and both end with a newline, which is the
// detail everybody forgets.
char *asctime(const struct tm *tm);
char *ctime(const time_t *t);

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *tm);

#endif
