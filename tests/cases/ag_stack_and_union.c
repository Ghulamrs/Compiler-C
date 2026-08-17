// expect: 0
/* Two aggregate-passing wrongs fable-5 review found on arm64: a stack-placed
   struct argument had its side effects run twice, and a union of doubles
   counted as a two-element HFA - the widest member is the count, since
   union members overlap. Checked against the reference compiler. */
#include <stdio.h>
static int n = 0;
struct S { long a, b; };
static struct S make(void) { n++; struct S s; s.a = 1; s.b = 2; return s; }
static long f(long a,long b,long c,long d,long e,long g,long h,long i, struct S s) {
    return a+b+c+d+e+g+h+i+s.a+s.b;
}
union U { double a; double b; };
static double g2(union U u, double after) { return u.a + after; }
int main(void) {
    /* Sequenced before the printf: the order printf's own arguments are
       evaluated in is unspecified, and reading n in the same call read it
       before the side effect on one compiler and after it on the other. */
    long r = f(1,2,3,4,5,6,7,8, make());
    printf("%ld %d\n", r, n);
    union U u; u.a = 2.5;
    printf("%.1f\n", g2(u, 0.25));
    return 0;
}
