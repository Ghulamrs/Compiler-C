// expect: 0
// size_t is typedefed in a header rather than answered by Target, so the width
// is a claim made in text. This is what checks the claim: gcc builds the same
// file against glibc's stddef.h, and sizeof(size_t) has to come out the same.
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    size_t n;
    void *p;

    n = sizeof(size_t);
    p = malloc(8);
    printf("%lu %d %d\n", n, p == NULL, p != NULL);
    free(p);
    return 0;
}
