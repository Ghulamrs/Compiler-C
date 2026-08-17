// expect: 0
/* Converting a 64-bit unsigned value to floating point, which is not the
   signed conversion the hardware instruction performs. cvtsi2sd reads its
   operand as signed, so anything with the top bit set arrives negative:
   ~0UL came out -1.0 where it is 18446744073709551615.0.

   Only the 64-bit unsigned types can reach that bit. Everything narrower has
   already been widened into the full register and converts correctly as
   signed, so those are here to hold that half of the rule down. */
#include <stdio.h>

int main(void)
{
    unsigned long long big = 18446744073709551615ULL;   /* 2^64 - 1 */
    unsigned long long half = 9223372036854775808ULL;   /* 2^63     */
    unsigned int narrow = 4294967295u;
    unsigned long ul = 1234567890123456789UL;

    double d1 = (double)big;
    double d2 = (double)half;
    double d3 = (double)narrow;
    float  f1 = (float)half;
    double d4 = (double)ul;

    printf("%.1f\n", d1);
    printf("%.1f\n", d2);
    printf("%.1f\n", d3);
    printf("%.1f\n", (double)f1);
    printf("%.1f\n", d4);

    /* The round trip, which is what a program actually does with these. */
    printf("%d\n", (double)big > 1.8e19);
    printf("%d\n", (double)half == 9223372036854775808.0);

    /* A signed value with the same bits must still convert as signed. */
    printf("%.1f\n", (double)(long long)-1);
    return 0;
}
