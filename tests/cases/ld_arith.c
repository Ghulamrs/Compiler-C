// expect: 0
// 'long double' as arithmetic: the four operators, all nine comparisons, the
// unary minus, ++ and --, and the conversions in and out.
//
// On x86_64-linux this is x87's 80-bit format and none of it is SSE, so every
// line here exercises a different piece of a code generator that no other type
// reaches. On the other two targets long double is double and the same source
// is checking that the spelling costs nothing.
//
// The value to watch is 1.0L/3.0L printed to twenty places. A double gives
// 0.33333333333333331483; the answer below has four more correct digits, which
// is the only way from inside the language to tell 64 bits of significand from
// 53.
#include <stdio.h>
#include <float.h>

int main(void) {
    long double a = 7.0L, b = 2.0L;
    long double r;
    int bad = 0;

    if (a + b != 9.0L)  bad++;
    if (a - b != 5.0L)  bad++;
    if (a * b != 14.0L) bad++;
    if (a / b != 3.5L)  bad++;
    if (-a != -7.0L)    bad++;

    // The order matters and is the thing that was wrong first: with the
    // subtract and divide written the other way round these read -5 and 0.2857.
    printf("sub=%.4Lf div=%.4Lf\n", a - b, a / b);

    if (a <  b) bad++;
    if (a <= b) bad++;
    if (!(a >  b)) bad++;
    if (!(a >= b)) bad++;
    if (a == b) bad++;
    if (!(a != b)) bad++;
    if (!(b <= b) || !(b >= b) || !(b == b)) bad++;

    r = 1.0L;
    r++;
    if (r != 2.0L) bad++;
    r--;
    if (r != 1.0L) bad++;

    // Widening keeps the value exactly; narrowing rounds once.
    if ((long double)2.5 != 2.5L) bad++;
    if ((double)2.5L != 2.5) bad++;
    if ((long double)7 != 7.0L) bad++;
    if ((int)3.75L != 3) bad++;          // toward zero, not to nearest
    if ((int)-3.75L != -3) bad++;
    if ((long)1234567890123L != 1234567890123L) bad++;

    // Truth, and the operators that consult it.
    if (!a) bad++;
    if (!(!(a - a))) bad++;
    if ((a && b) != 1) bad++;
    if ((0.0L || b) != 1) bad++;

    printf("third=%.20Lf\n", 1.0L / 3.0L);
    printf("mant=%d dig=%d\n", LDBL_MANT_DIG, LDBL_DIG);
    printf("wider=%d\n", (int)(sizeof(long double) >= sizeof(double)));

    return bad;
}
