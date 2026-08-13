// The integer type system: three character types, every width signed and
// unsigned, the promotions, and the rule that signedness picks the instruction
// rather than only the type.
#include <stdio.h>
#include "examples.h"

int ex_arithmetic(void) {
    signed char sc = -1;
    unsigned char uc = 255;
    short s = -300;
    unsigned short us = 65535u;
    int i = -7;
    unsigned int ui = 4294967295u;
    long l = -1234567890123L;
    /* Not ULONG_MAX: an integer literal is carried in a signed long here,
       so the largest unsigned values cannot be written as constants yet. */
    unsigned long ul = 9223372036854775807UL;

    printf("[arithmetic]\n");
    /* Five values a call, because a format string is itself one of the six
       integer argument registers System V provides. */
    printf("  widths   : char %d short %d int %d\n", (int)sizeof(char),
           (int)sizeof(short), (int)sizeof(int));
    printf("             long %d float %d double %d\n", (int)sizeof(long),
           (int)sizeof(float), (int)sizeof(double));
    printf("  chars    : %d %u\n", sc, uc);
    printf("  shorts   : %d %u\n", s, us);
    printf("  ints     : %d %u\n", i, ui);
    printf("  longs    : %ld %lu\n", l, ul);

    // Signedness decides the comparison, which is why the first is false.
    printf("  -1 < 1u  : %d\n", -1 < 1u);
    printf("  -1L < 1u : %d\n", -1L < 1u);
    printf("  -1 >> 1  : %d\n", -1 >> 1);
    printf("  4294967295u >> 1 : %u\n", 4294967295u >> 1);

    printf("  div/mod  : %d %d %d %d\n", 17 / 5, 17 % 5, -17 / 5, -17 % 5);
    printf("  bitwise  : %d %d %d %d\n", 12 & 10, 12 | 10, 12 ^ 10, ~12);
    printf("  a&b==c   : %d\n", 1 & 1 == 1);
    return 12;
}
