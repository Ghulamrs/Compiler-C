// expect: 0
/* float and double on arm64: the two views of one vector register. A float
   lives in s0 and a double in d0, and naming the wrong one assembles fine and
   computes at the wrong precision - which is why fpReg() exists rather than a
   literal "d0" at each site.

   'x += 1.0' is the case worth watching: x is a float, so C promotes it to
   double, adds, and converts back on the assignment. Two fcvt instructions
   around one fadd, and doing the add in single precision gives other bits. */
int printf(const char *, ...);

int main(void)
{
    float  x = 0.0f;
    double y = 0.0;
    double z = 0;
    int i;

    for (i = 0; i < 4; i++) {
        x += 1.0;
        y += 2.0;
        z += x + y;
        printf("%2d. %5.1f %5.1f %7.1lf\n", i, x, y, z);
    }

    printf("int to double : %f\n", (double)7);
    printf("double to int : %d\n", (int)3.9);
    printf("double to int : %d\n", (int)-3.9);
    printf("float to double: %f\n", (double)0.5f);
    printf("unsigned conv : %f\n", (double)4000000000u);
    printf("negate        : %f\n", -y);
    printf("divide        : %f\n", y / 4.0);
    return 0;
}
