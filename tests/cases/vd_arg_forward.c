// expect: 0
// The walk handed to the C library rather than consumed here. vprintf reads
// the va_list with the platform's own idea of its layout, so this is the one
// case where being wrong about the record is caught by something that was not
// compiled by cc1 - a walk that is self-consistent but does not match the ABI
// passes every other test in this file and fails this one.
//
// The va_list is also read twice, from two separate starts, which is what the
// standard allows and what a walk that mutated shared state would break.
#include <stdarg.h>
#include <stdio.h>

static int report(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

static int twice(int n, ...)
{
    va_list a, b;
    int t = 0, i;
    va_start(a, n);
    va_start(b, n);
    for (i = 0; i < n; i++) t += va_arg(a, int);
    for (i = 0; i < n; i++) t += va_arg(b, int);
    va_end(a);
    va_end(b);
    return t;
}

int main(void)
{
    /* "42 hi 2.5\n" is ten characters */
    return (report("%d %s %g\n", 42, "hi", 2.5) - 10)
         + (twice(3, 1, 2, 3) - 12);
}
