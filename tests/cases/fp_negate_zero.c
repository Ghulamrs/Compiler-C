// expect: 0
/* Unary minus on floating point, which is a sign flip and not a subtraction
   from zero. '0.0 - x' answers +0.0 when x is -0.0, so negation used to lose
   the sign of a zero - and with it the sign of the infinity that dividing by
   one produces, which is the way a program notices.

   printf prints the sign of a zero, so the first two lines alone would have
   caught it; there was simply no case that asked. */
#include <stdio.h>

static double negate(double x) { return -x; }
static float  negatef(float x) { return -x; }

int main(void)
{
    double z = 0.0;
    double nz = -z;
    float fz = 0.0f;

    printf("%.1f\n", nz);
    printf("%.1f\n", -nz);
    printf("%.1f\n", (double)-fz);

    /* 1.0/-0.0 is -inf and 1.0/+0.0 is +inf, which is the difference the sign
       of a zero actually makes. Compared rather than printed, because the
       spelling of infinity is the library's business. */
    printf("%d\n", 1.0 / nz < 0.0);
    printf("%d\n", 1.0 / z > 0.0);
    printf("%d\n", 1.0 / negate(0.0) < 0.0);
    printf("%d\n", 1.0 / (double)negatef(0.0f) < 0.0);

    /* Negation of an ordinary value, twice, still has to be the identity. */
    printf("%.2f\n", -(-2.75));
    printf("%.2f\n", (double)-(-2.75f));
    printf("%.2f\n", negate(3.5));
    return 0;
}
