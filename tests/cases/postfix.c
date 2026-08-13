// expect: 0
// x++ and x--, whose value is what the object held before the store.
//
// One side effect per statement, deliberately. "printf("%d %d", c++, c)" reads c
// twice in one call and C leaves the order of arguments unspecified - gcc
// evaluates right to left and this compiler left to right, so a case written
// that way would assert something the standard does not promise.
//
// The narrow types are the point of the exercise. "(x += 1) - 1" is the obvious
// way to build postfix out of what already existed, and every line below with a
// type that wraps is a line where that rewrite gives the wrong answer.
#include <stdio.h>

struct P { int x; };

int main(void) {
    int i = 5, j;
    unsigned char c = 255;  int cu;
    char sc = 127;          int cs;
    short s = 32767;        int ss;
    double d = 1.5, dd;
    float f = 2.5f, ff;
    int v[5];
    int *p, pv;
    struct P a[3];
    int k, m;

    j = i++;   printf("int++  : yielded %d, now %d\n", j, i);
    j = i--;   printf("int--  : yielded %d, now %d\n", j, i);

    cu = c++;  printf("uchar  : yielded %d, now %d\n", cu, c);
    cs = sc++; printf("schar  : yielded %d, now %d\n", cs, sc);
    ss = s++;  printf("short  : yielded %d, now %d\n", ss, s);

    dd = d++;  printf("double : yielded %.1f, now %.1f\n", dd, d);
    ff = f++;  printf("float  : yielded %.1f, now %.1f\n", ff, f);
    dd = d--;  printf("double-: yielded %.1f, now %.1f\n", dd, d);

    for (k = 0; k < 5; k = k + 1) v[k] = k * 10;
    p = v;
    pv = *p++; printf("ptr++  : yielded %d, now %d\n", pv, *p);
    pv = *p--; printf("ptr--  : yielded %d, now %d\n", pv, *p);

    a[0].x = 1; a[1].x = 2; a[2].x = 3;
    k = 0;
    m = a[k++].x; printf("index  : %d then k=%d\n", m, k);

    k = 0;
    while (k++ < 3) ;
    printf("loop   : %d\n", k);

    k = 0;
    v[k++] = 99;
    printf("lvalue : v[0]=%d k=%d\n", v[0], k);

    k = 3; m = 0;
    while (k-- > 0) m = m + k;
    printf("count  : k=%d m=%d\n", k, m);
    return 0;
}
