// expect: 0
// <stdio.h> as cc1 ships it, against gcc reading glibc's. The suite does not
// pass -I to gcc, so the two builds read different headers on purpose: a
// prototype that disagreed with the real one would show up here as different
// output rather than as a compile error nobody would see.
#include <stdio.h>

int main(void) {
    printf("%d %s %c\n", 42, "text", 'x');
    puts("a line");
    putchar('z');
    putchar('\n');
    printf("%.3f %ld %u\n", 1.5, 100000L, 7u);
    return 0;
}
