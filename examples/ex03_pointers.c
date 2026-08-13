// Pointers and arrays: taking an address, dereferencing, arithmetic that scales
// by the element, decay at a use but not under sizeof, and a matrix reached
// through a cast of malloc's block.
#include <stdio.h>
#include <stdlib.h>
#include "examples.h"

static void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }

static int measure(char s[16]) { return (int)sizeof s; }   /* a pointer, so 8 */

int ex_pointers(void) {
    int v[6];
    int *p;
    int (*matrix)[4];
    char text[16];
    int i, j;

    printf("[pointers]\n");
    for (i = 0; i < 6; i = i + 1) v[i] = i * i;
    p = v;
    printf("  deref    : %d %d %d\n", *p, *(p + 3), p[5]);
    printf("  scaling  : %d\n", (int)(&v[4] - &v[1]));
    printf("  i[a]     : %d\n", 2[v]);

    swap(&v[0], &v[5]);
    printf("  swapped  : %d %d\n", v[0], v[5]);

    printf("  sizeof   : %d %d\n", (int)sizeof v, (int)sizeof p);
    printf("  as param : %d (an array parameter is a pointer)\n", measure(text));

    // A pointer to an array of four, which is what turns a flat block into a
    // matrix - and the reason declarators have to be recursive.
    matrix = (int (*)[4])malloc(3 * 4 * sizeof(int));
    for (i = 0; i < 3; i = i + 1)
        for (j = 0; j < 4; j = j + 1) matrix[i][j] = i * 4 + j;
    printf("  matrix   : %d %d %d\n", matrix[0][0], matrix[1][2], matrix[2][3]);
    free(matrix);

    p = 0;
    printf("  null     : %d\n", p == 0);
    return 9;
}
