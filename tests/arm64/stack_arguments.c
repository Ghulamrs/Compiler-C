// expect: 0
// Arguments past the eight registers, which on Apple is not the layout the
// AAPCS64 document describes. The standard gives every stack argument an
// eight-byte slot; Apple gives it its own size at its own alignment, so four
// ints past the registers occupy sixteen bytes and not thirty-two, and a char
// followed by an int followed by a long sits at 0, 4 and 8.
//
// Judged against clang, which is the only thing that can settle it: a caller
// and a callee that are both wrong in the same way agree with each other
// perfectly and produce nonsense against anything else. `widths` below is the
// case that pins the alignment rule.
#include <stdarg.h>
#include <stdio.h>

static int twelve(int a, int b, int c, int d, int e, int f,
                  int g, int h, int i, int j, int k, int l)
{
    return a + b*2 + c*3 + d*4 + e*5 + f*6 + g*7 + h*8 + i*9 + j*10 + k*11 + l*12;
}

// Mixed widths past the registers. Every one of these lands at a different
// offset under Apple's rule than under the standard's.
static long widths(int a, int b, int c, int d, int e, int f, int g, int h,
                   char c1, short s1, int n1, long l1, char c2, double d1)
{
    return (long)c1 + s1*2 + n1*3 + l1*4 + c2*5 + (long)d1*6
         + a + b + c + d + e + f + g + h;
}

static double tenD(double a, double b, double c, double d, double e,
                   double f, double g, double h, double i, double j)
{
    return a + b*2 + c*3 + d*4 + e*5 + f*6 + g*7 + h*8 + i*9 + j*10;
}

// The two files are counted independently, so this exhausts them at different
// points and each overflows on its own.
static int crossed(int a, double b, int c, double d, int e, double f,
                   int g, double h, int i, double j, int k, double l)
{
    return a + c + e + g + i + k + (int)(b + d + f + h + j + l);
}

// Eleven named parameters, three of them past the registers, and then a
// variadic part - so the variadic slots do not begin at x29+16, and va_start
// has to know how far the named ones pushed them along.
static long after_stack_named(int a, int b, int c, int d, int e, int f,
                              int g, int h, int i, int j, int n, ...)
{
    va_list ap;
    long t = a + b + c + d + e + f + g + h + i + j;
    int m;
    va_start(ap, n);
    for (m = 0; m < n; m++) t += va_arg(ap, int);
    va_end(ap);
    return t;
}

int main(void)
{
    int (*p)(int, int, int, int, int, int, int, int, int, int, int, int) = twelve;

    return (twelve(1,2,3,4,5,6,7,8,9,10,11,12) - 650)
         + (int)(widths(1,1,1,1,1,1,1,1, 'A', 300, 7, 1000L, 'B', 2.0) - 5036)
         + (int)(tenD(1,2,3,4,5,6,7,8,9,10) - 385.0)
         + (crossed(1,1.5,2,2.5,3,3.5,4,4.5,5,5.5,6,6.5) - 45)
         + (int)(after_stack_named(1,2,3,4,5,6,7,8,9,10, 3, 100,200,300) - 655)
         + (p(1,2,3,4,5,6,7,8,9,10,11,12) - 650);
}
