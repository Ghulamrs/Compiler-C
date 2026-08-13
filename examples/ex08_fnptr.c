// Pointers to functions: declared, assigned, passed, held in a table, and given
// to libc's own qsort - which then calls back into code this compiler made.
#include <stdio.h>
#include <stdlib.h>
#include "examples.h"

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

static int apply(int (*op)(int, int), int a, int b) { return op(a, b); }

typedef int (*BinOp)(int, int);

static int cmp_int(const void *a, const void *b) {
    int x = *(int *)a;
    int y = *(int *)b;

    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int ex_fnptr(void) {
    int (*f)(int, int);
    BinOp table[3];
    int v[7];
    int key;
    int *found;
    int i;

    printf("[fnptr]\n");
    f = add;
    printf("  direct   : %d\n", f(3, 4));
    f = sub;
    printf("  reseated : %d\n", f(10, 4));

    // The name of a function is a pointer to it, with no '&' written.
    printf("  passed   : %d %d\n", apply(mul, 6, 7), apply(add, 1, 2));

    table[0] = add; table[1] = sub; table[2] = mul;
    printf("  table    :");
    for (i = 0; i < 3; i = i + 1) printf(" %d", table[i](12, 3));
    printf("\n");

    printf("  compared : %d %d size=%d\n", f == sub, f == add, (int)sizeof f);

    v[0] = 42; v[1] = 7; v[2] = 19; v[3] = 3; v[4] = 88; v[5] = 1; v[6] = 55;
    qsort(v, 7, sizeof(int), cmp_int);
    printf("  qsort    :");
    for (i = 0; i < 7; i = i + 1) printf(" %d", v[i]);
    printf("\n");

    key = 19;
    found = (int *)bsearch(&key, v, 7, sizeof(int), cmp_int);
    printf("  bsearch  : %d at %d\n", *found, (int)(found - v));
    return 8;
}
