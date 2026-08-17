// expect: 0
/* Arithmetic in 'float' rather than 'double', and the conversions either side
   of it. Ordinary C, and every operation here reaches an instruction the
   corpus had never asked for: the whole corpus computed in double, so subss,
   mulss, divss, cvtsi2ssq and cvttss2si were emitted by nothing.

   That mattered beyond coverage. The MASM spelling refuses an instruction it
   has no rule for rather than guessing, and had rules for the double forms
   only - so 'a - b' on two floats stopped the compiler on x86_64-windows
   while the same line in double compiled. A table fitted to the corpus is a
   statement about the corpus, not about the language. */
#include <stdio.h>

static float diff(float a, float b) { return a - b; }
static float prod(float a, float b) { return a * b; }
static float quot(float a, float b) { return a / b; }

int main(void)
{
    float a = 7.5f, b = 2.5f;

    printf("%.2f\n", (double)(a + b));
    printf("%.2f\n", (double)diff(a, b));
    printf("%.2f\n", (double)prod(a, b));
    printf("%.2f\n", (double)quot(a, b));

    /* int to float and back, which are the cvtsi2ss and cvttss2si pair. */
    printf("%d\n", (int)(float)42);
    printf("%d\n", (int)quot(a, b));
    printf("%.2f\n", (double)(float)-13);

    /* float against float, so the comparison is ucomiss and not ucomisd. */
    printf("%d\n", a > b);
    printf("%d\n", diff(a, b) == 5.0f);

    /* A 64-bit signed divide, whose cqo the Windows table also lacked. */
    {
        long long n = -9007199254740993LL, d = 3LL;
        printf("%lld\n", n / d);
        printf("%lld\n", n % d);
    }
    return 0;
}
