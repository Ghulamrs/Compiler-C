// expect: 0
// strtod, which is <stdlib.h>'s number reader that can say it failed. atol
// cannot: it answers zero for "0" and for "banana" alike, and a program
// reading a number it did not write itself has no way to tell the two apart.
// strtod hands back the rest of the string, and a conversion that found
// nothing leaves that pointer where it started - which is the whole of how a
// failure is reported.
//
// Every value here is exact in binary, so the two compilers cannot disagree
// about the last digit of a printf: halves, quarters and powers of ten.
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *rest;
    double d;

    d = strtod("3.5 apples", &rest);
    if (d != 3.5) return 1;
    if (rest[0] != ' ' || rest[1] != 'a') return 2;

    /* Leading space is skipped, a sign is taken, and an exponent is read. */
    if (strtod("  -0.25", &rest) != -0.25) return 3;
    if (*rest != '\0') return 4;
    if (strtod("1e3", &rest) != 1000.0) return 5;

    /* Nothing to convert: zero, and the pointer back at the beginning. That
       equality is the check - a failure that moved the pointer would be a
       failure a caller cannot see. */
    {
        const char *text = "banana";
        d = strtod(text, &rest);
        if (d != 0.0) return 6;
        if (rest != text) return 7;
    }

    /* And a number followed by nothing leaves the pointer on the terminator,
       which is how a caller knows the whole string was a number. */
    d = strtod("64", &rest);
    if (d != 64.0 || *rest != '\0') return 8;

    printf("%.2f %.2f %.0f\n", 3.5, -0.25, 1000.0);
    return 0;
}
