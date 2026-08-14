// expect: 0
/* Named parameters come out of the same registers a fixed function would use,
   and the variadic walk has to begin after them - gp_offset counts them. Five
   named integers leave exactly one integer register for the variadic part, so
   an off-by-one in that count is visible rather than absorbed. */
#include <stdarg.h>
int vprintf(const char *, va_list);

int five(int a, int b, int c, int d, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    return a + b + c + d;
}

int main(void)
{
    int s = five(1, 2, 3, 4, "%d %d %d\n", 10, 20, 30);
    return s - 10;
}
