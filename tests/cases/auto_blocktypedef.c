// expect: 0
// 'auto' is the default storage class for a local, so it means nothing - which
// is exactly why it was the last of ANSI C's 32 keywords still refused, and why
// the refusal read 'auto was not declared'. atDeclarationStart had never listed
// it, so the declaration was read as a statement starting with an unknown name,
// and the message blamed the program for a gap in the compiler.
#include <stdio.h>

int shared;

int scale(register int n) {
    auto int doubled;
    doubled = n * 2;
    return doubled;
}

typedef int Counter;

int main(void) {
    auto Counter i;
    auto int a[4];
    auto double d;
    auto struct { int x, y; } p;
    Counter total;

    total = 0;
    for (i = 0; i < 4; i++) {
        a[i] = i * 3;
        total += a[i];
    }
    printf("loop  : %d %d %d\n", a[1], a[3], total);

    p.x = 2;
    p.y = 5;
    printf("point : %d\n", p.x * p.y);

    d = 1.5;
    printf("auto d: %.1f\n", d * 2.0);

    printf("scale : %d\n", scale(21));

    {
        extern int shared;
        shared = 9;
    }
    printf("extern: %d\n", shared);
    return 0;
}
