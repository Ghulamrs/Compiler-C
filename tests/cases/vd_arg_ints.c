// expect: 0
// Nine integers through va_arg. Six of them were passed in registers and
// spilled to the save area; the rest never were, and come from the overflow
// area instead. The count is deliberate - eight would not cross the boundary,
// and a walk that only ever reads the save area passes every shorter test.
#include <stdarg.h>

static int total(int n, ...)
{
    va_list ap;
    int t = 0, i;
    va_start(ap, n);
    for (i = 0; i < n; i++) t += va_arg(ap, int);
    va_end(ap);
    return t;
}

int main(void)
{
    /* 1+..+9 = 45, and the first six are the ones in registers */
    return (total(9, 1, 2, 3, 4, 5, 6, 7, 8, 9) - 45)
         + (total(1, 7) - 7)
         + (total(0) - 0);
}
