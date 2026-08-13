// expect: 0
// qsort and bsearch through the shipped <stdlib.h>, which is what pointers to
// functions were wanted for. gcc builds this against glibc's header, so the
// declaration of qsort here has to agree with the real one down to the type of
// the comparison function - and the sort is done by libc itself, calling back
// into code this compiler generated.
#include <stdio.h>
#include <stdlib.h>

int cmp_int(const void *a, const void *b) {
    int x = *(int *)a;
    int y = *(int *)b;

    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

// A comparison written in terms of another, so the callback itself calls
// through a pointer's worth of indirection more than once.
int cmp_desc(const void *a, const void *b) {
    return cmp_int(b, a);
}

int main(void) {
    int v[8];
    int key;
    int *found;
    int i;

    v[0] = 42; v[1] = 7;  v[2] = 19; v[3] = 3;
    v[4] = 88; v[5] = 1;  v[6] = 55; v[7] = 12;

    qsort(v, 8, sizeof(int), cmp_int);
    printf("asc :");
    i = 0;
    while (i < 8) { printf(" %d", v[i]); i = i + 1; }
    printf("\n");

    key = 19;
    found = (int *)bsearch(&key, v, 8, sizeof(int), cmp_int);
    printf("find: %d at %d\n", *found, (int)(found - v));

    qsort(v, 8, sizeof(int), cmp_desc);
    printf("desc:");
    i = 0;
    while (i < 8) { printf(" %d", v[i]); i = i + 1; }
    printf("\n");
    return 0;
}
