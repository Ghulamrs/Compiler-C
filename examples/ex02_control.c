// Every statement the compiler has: if/else, the three loops, switch with
// fallthrough, break, continue, goto, and the comma and conditional operators.
#include <stdio.h>
#include "examples.h"

static int classify(int n) {
    switch (n) {
    case 0:  return 100;
    case 1:
    case 2:  return 200;          /* two labels, one arm */
    case 3:  n = n + 10;          /* falls through on purpose */
    case 4:  return 300 + n;
    default: return 999;
    }
}

int ex_control(void) {
    int i, s, n;

    printf("[control]\n");

    s = 0;
    for (i = 0; i < 5; i = i + 1) s = s + i;
    printf("  for      : %d\n", s);

    s = 0; i = 0;
    while (i < 5) { s = s + i; i = i + 1; }
    printf("  while    : %d\n", s);

    s = 0; i = 0;
    do { s = s + i; i = i + 1; } while (i < 5);
    printf("  do-while : %d\n", s);

    n = 0;
    do { n = n + 1; } while (0);
    printf("  do runs once when false: %d\n", n);

    s = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i == 3) continue;
        if (i == 7) break;
        s = s + i;
    }
    printf("  break/continue: %d\n", s);

    printf("  switch   : %d %d %d %d %d\n", classify(0), classify(1), classify(2),
           classify(3), classify(9));

    i = 0;
    s = 0;
again:
    s = s + i;
    i = i + 1;
    if (i < 4) goto again;
    printf("  goto     : %d\n", s);

    n = 5;
    printf("  ternary  : %d %.1f\n", n > 3 ? 1 : 0, n ? 1 : 2.5);
    printf("  comma    : %d\n", (i = 1, i + 1));
    printf("  shortcut : %d %d\n", 0 && 1 / 0, 1 || 1 / 0);
    return 11;
}
