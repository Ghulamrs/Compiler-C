// expect: 0
/* The other two predefined macros C90 asks for, beside __FILE__, __LINE__ and
   __STDC__. Both are string literals of a fixed shape and neither can be
   checked against a value: this file is compiled by two compilers a moment
   apart, and __TIME__ would differ between them by the second that passed.
   The shape is what both must agree on, and it is enough to catch the two
   ways of getting __DATE__ wrong - a zero where C says a space, and a
   locale's own month name. */
#include <stdio.h>

static int length(const char *s)
{
    int n = 0;
    while (s[n] != '\0') n++;
    return n;
}

int main(void)
{
    /* "Mmm dd yyyy" and "hh:mm:ss", always. */
    if (length(__DATE__) != 11) return 1;
    if (length(__TIME__) != 8) return 2;

    if (__DATE__[3] != ' ' || __DATE__[6] != ' ') return 3;
    if (__TIME__[2] != ':' || __TIME__[5] != ':') return 4;

    /* A day below ten is padded with a space, not a zero - which is the half
       of the format that is always got wrong. Only one of these two can be
       true on any given day, and neither may be a zero in that column. */
    if (__DATE__[4] != ' ' && (__DATE__[4] < '1' || __DATE__[4] > '3')) return 5;
    if (__DATE__[5] < '0' || __DATE__[5] > '9') return 6;

    /* The month is one of the twelve English abbreviations, whatever locale
       the machine is set to. */
    {
        /* An array of characters rather than a pointer to a literal: a const
           object holding the address of one is put in __TEXT,__const for
           arm64-darwin and the Mach-O linker refuses the text relocation. That
           is a fault of its own and nothing to do with these two macros, so
           this case does not stand on it. */
        static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
        int at = 0;
        int found = 0;
        while (months[at] != '\0') {
            if (months[at] == __DATE__[0] && months[at + 1] == __DATE__[1] &&
                months[at + 2] == __DATE__[2]) found = 1;
            at += 3;
        }
        if (!found) return 7;
    }

    /* And the same for every use in one translation unit, which is what makes
       them worth predefining rather than asking the clock twice. */
    {
        const char *first = __TIME__;
        const char *second = __TIME__;
        int i = 0;
        while (i < 8) {
            if (first[i] != second[i]) return 8;
            i++;
        }
    }

    printf("__DATE__ and __TIME__ have the shape C asks for\n");
    return 0;
}
