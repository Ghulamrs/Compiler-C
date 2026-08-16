// locale.h - the conventions that vary by country.
//
// The LC_ numbers are the thing to be careful about here, because they are the
// one set in the standard library that the three platforms genuinely disagree
// about. Measured:
//
//                LC_ALL  LC_COLLATE  LC_CTYPE  LC_MONETARY  LC_NUMERIC  LC_TIME
//     macOS         0         1          2          3            4         5
//     Windows       0         1          2          3            4         5
//     glibc         6         3          0          4            1         2
//
// A program calling setlocale(LC_ALL, "") with the wrong number does not fail;
// it sets a different category and carries on, which is why this is guarded
// rather than picked.
//
// struct lconv is the other half, and it is a **read-only** structure: the
// program never declares one, it takes the pointer localeconv returns and
// reads through it. That is what makes it safe for this declaration to be
// shorter than the platform's own - the C90 members come first on all three,
// so their offsets are right, and the wide-character members the UCRT adds
// after them are simply not described here. A struct tm could not be treated
// this way, because a program allocates one of those.
#ifndef _CC1_LOCALE_H
#define _CC1_LOCALE_H

#include <stddef.h>

#ifdef __linux__
#define LC_CTYPE     0
#define LC_NUMERIC   1
#define LC_TIME      2
#define LC_COLLATE   3
#define LC_MONETARY  4
#define LC_ALL       6
#else
#define LC_ALL       0
#define LC_COLLATE   1
#define LC_CTYPE     2
#define LC_MONETARY  3
#define LC_NUMERIC   4
#define LC_TIME      5
#endif

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};

// Returns the locale now in effect, or null if the one asked for is not
// available. "" means the environment's, "C" the minimal one every program
// starts in. The string it returns is the library's, not yours.
char *setlocale(int category, const char *locale);

struct lconv *localeconv(void);

#endif
