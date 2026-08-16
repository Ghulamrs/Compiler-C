// expect: 0
// The standard headers doing the thing they exist for, rather than merely
// being includable. Every check below is one the platform's own library
// answers, so a prototype that disagreed with it - a wrong struct size, a
// wrong macro number, an errno that is a variable rather than a function -
// comes out as a wrong answer here rather than as a link error.
//
// <setjmp.h> is here too, but only far enough to show it is declared and
// links. What it is actually for - surviving a longjmp back into an
// assignment - is hd_setjmp.c, which needs a scribbled-on stack to assert
// anything and does not belong in a list of one-line checks.
//
// It was behind an #ifdef until x86_64-windows learned this header too, and
// is not any more: all fifteen work on all three targets now.
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <locale.h>
#include <setjmp.h>

static volatile sig_atomic_t caught = 0;
static void onsig(int s) { (void)s; caught = 1; }

int main(void)
{
    int bad = 0;
    struct tm t;
    time_t tt;
    char buf[64];
    struct lconv *lc;
    void (*prev)(int);
    jmp_buf jb;
    volatile int roundtrip = 0;

    // <limits.h> and <float.h>. DBL_EPSILON is checked by what it means -
    // the gap above 1.0 - rather than by its digits, which is the only way to
    // tell a right value from a plausible one.
    if (CHAR_BIT != 8 || SCHAR_MIN != -128 || UCHAR_MAX != 255) bad++;
    if (INT_MAX != 2147483647 || SHRT_MAX != 32767) bad++;
    if (sizeof(long) == 8 && LONG_MAX != 9223372036854775807L) bad++;
    if (sizeof(long) == 4 && LONG_MAX != 2147483647L) bad++;
    if (DBL_DIG != 15 || FLT_DIG != 6 || DBL_MANT_DIG != 53) bad++;
    if (1.0 + DBL_EPSILON == 1.0) bad++;
    if (1.0 + DBL_EPSILON / 2.0 != 1.0) bad++;

    // <ctype.h>
    if (!isdigit('7') || isdigit('x') || !isalpha('Q') || !isspace(' ')) bad++;
    if (!isupper('Q') || !islower('q') || !isxdigit('f') || ispunct('a')) bad++;
    if (toupper('a') != 'A' || tolower('Z') != 'z') bad++;

    // <errno.h> - set by a real failure, which is what proves errno is the
    // platform's own thread-local and not a variable this header invented.
    errno = 0;
    if (fopen("/no/such/path/anywhere/at/all", "r") != NULL) bad++;
    if (errno == 0) bad++;
    errno = 0;

    // <assert.h> - the passing side. The failing side ends the process, so it
    // is not something a test in this corpus can take.
    assert(1 == 1);
    assert(CHAR_BIT == 8);

    // <signal.h> - installed, raised, and the handler seen to have run.
    prev = signal(SIGTERM, onsig);
    if (prev == SIG_ERR) bad++;
    else {
        raise(SIGTERM);
        if (!caught) bad++;
        signal(SIGTERM, prev);
    }

    // <time.h> - a date normalised by mktime and formatted back out. This is
    // the one that would catch a struct tm of the wrong size, because mktime
    // writes tm_wday and tm_yday into the caller's object.
    memset(&t, 0, sizeof t);
    t.tm_year = 126;              /* 2026 */
    t.tm_mon  = 7;                /* August */
    t.tm_mday = 16;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    tt = mktime(&t);
    if (tt == (time_t)-1) bad++;
    if (t.tm_wday < 0 || t.tm_wday > 6) bad++;
    if (t.tm_yday != 227) bad++;  /* 16 August is day 227 of a non-leap year */
    if (strftime(buf, sizeof buf, "%Y-%m-%d", &t) == 0) bad++;
    if (strcmp(buf, "2026-08-16") != 0) bad++;
    if (localtime(&tt) == NULL) bad++;
    if (difftime(tt + 60, tt) != 60.0) bad++;

    // <locale.h> - the C locale is the one every program starts in, and its
    // decimal point is "." on all three platforms whatever LC_ALL is numbered.
    if (setlocale(LC_ALL, "C") == NULL) bad++;
    lc = localeconv();
    if (lc == NULL || lc->decimal_point == NULL) bad++;
    else if (strcmp(lc->decimal_point, ".") != 0) bad++;

    // <setjmp.h> - a round trip, which is as much as belongs here. jmp_buf
    // being too small would be caught by the library writing past its end
    // rather than by anything this file could ask.
    if (setjmp(jb) == 0) longjmp(jb, 5);
    else                 roundtrip = 1;
    if (!roundtrip) bad++;

    return bad;
}
