// expect: 0
// The callee half of the variadic part. variadic_printf.c beside this file
// covers the caller - handing arguments to printf - and this one covers the
// walk back out of them, which is the half Apple's departure from AAPCS64
// makes simple: every variadic argument is on the stack in an eight-byte slot,
// so the va_list is a pointer and never a register save area.
//
// Judged against clang rather than against an expected number alone, which is
// what matters here: the layout has to be the platform's, not merely
// self-consistent, and vprintf below reads it with the system's own idea of
// what a va_list is.
#include <stdarg.h>
#include <stdio.h>

// Eight named parameters spend every integer register before the variadic part
// starts, which is where an offset computed from the wrong base would show.
static long after_eight(int a, int b, int c, int d, int e, int f, int g, int h, ...)
{
    va_list ap;
    long t = a + b + c + d + e + f + g + h;
    int i;
    va_start(ap, h);
    for (i = 0; i < 3; i++) t += va_arg(ap, int);
    va_end(ap);
    return t;
}

// Interleaved types. Apple puts them all on the stack in order, so a walk that
// kept a separate cursor per type - which is what System V needs - would read
// these in the wrong places.
static double interleaved(int pairs, ...)
{
    va_list ap;
    double t = 0;
    int i;
    va_start(ap, pairs);
    for (i = 0; i < pairs; i++) {
        t += (double)va_arg(ap, int);
        t += va_arg(ap, double);
    }
    va_end(ap);
    return t;
}

// A va_list handed to another function, which forwards it to the C library.
static int inner(const char *fmt, va_list ap) { return vprintf(fmt, ap); }

static int outer(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = inner(fmt, ap);
    va_end(ap);
    return n;
}

int main(void)
{
    long a = after_eight(1, 2, 3, 4, 5, 6, 7, 8, 100, 200, 300);
    double b = interleaved(4, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5);
    int c = outer("%d %s %g %ld\n", 7, "x", 1.25, 99L);

    printf("%ld %g %d\n", a, b, c);

    /* 36 + 600 = 636; 10 + 12.0 = 22.0; "7 x 1.25 99\n" is twelve characters */
    return (int)(a - 636) + (int)(b - 22.0) + (c - 12);
}
