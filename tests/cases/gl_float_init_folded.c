// expect: 0
/* File-scope floating initialisers, which C90 6.5.7 says may be arithmetic
   constant expressions - so the folder has to fold, and it has to apply the
   casts it walks through rather than skip them. 'double d = (float)0.1;' is
   the float-rounded value, exactly as it is inside a function; skipping the
   cast gave one expression two answers depending on storage duration. */
#include <stdio.h>

static double third = 1.0 / 3.0;
static double sum = 0.1 + 0.2;
static double scaled = 2.5 * 4.0 - 1.5;
static double rounded = (float)0.1;
static double truncated = (int)2.9;
static double negated = -(0.5 + 0.25);
static float narrow = (float)(1.0 / 3.0);

int main(void)
{
    /* The same expressions with automatic storage go through codegen; the
       two storage durations must answer identically. */
    double a_third = 1.0 / 3.0;
    double a_rounded = (float)0.1;

    printf("%d\n", third == a_third);
    printf("%d\n", rounded == a_rounded);
    printf("%.17g\n", sum);
    printf("%.1f\n", scaled);
    printf("%.1f\n", truncated);
    printf("%.2f\n", negated);
    printf("%d\n", (double)narrow == a_rounded * (1.0 / 0.1) / 10.0 * 1.0
                       ? narrow > 0.3f && narrow < 0.4f : narrow > 0.3f);
    return 0;
}
