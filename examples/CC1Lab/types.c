#include <stdio.h>
#include "types.h"

void print_numset(NumSet ns) {
    printf("NumSet values:\n");
    printf(" float=%.3f\n", ns.f);
    printf(" double=%.6f\n", ns.d);
    printf(" long=%ld\n", ns.l);
    printf(" long long=%lld\n", ns.ll);
    printf(" long double=%.10Lf\n", ns.ld);
}
