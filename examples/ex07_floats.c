// float and double in their own register file, the System V rule that integer
// and floating arguments are counted in separate lanes, and the %al count that
// a variadic callee reads.
#include <stdio.h>
#include "examples.h"

static double average(double a, double b, double c) { return (a + b + c) / 3.0; }

static double newton_sqrt(double x) {
    double g = x / 2.0;
    int i = 0;

    while (i < 20) { g = (g + x / g) / 2.0; i = i + 1; }
    return g;
}

int ex_floats(void) {
    float f = 1.5f;
    double d = 2.25;
    int n = 7;

    printf("[floats]\n");
    printf("  literals : %.2f %.2f\n", f, d);
    printf("  mixed    : %.4f\n", d * n);
    printf("  promote  : %.1f\n", 1 + 0.5);

    // Two lanes: 1 and 2 go in integer registers, 1.5 and 2.5 in SSE ones.
    printf("  lanes    : %d %.1f %d %.1f\n", 1, 1.5, 2, 2.5);

    printf("  average  : %.6f\n", average(1.0, 2.0, 4.0));
    printf("  sqrt(2)  : %.8f\n", newton_sqrt(2.0));
    printf("  compare  : %d %d\n", 1.5 < 2.0, 2.0 == 2.0);
    printf("  to int   : %d %d\n", (int)3.99, (int)-3.99);
    printf("  from int : %.1f\n", (double)n);

    // A conditional whose arms are int and double is a double, even when the
    // integer arm is the one taken.
    printf("  ternary  : %.1f\n", n ? 1 : 2.5);
    return 10;
}
