// expect: 0
// <stdlib.h>. malloc through a prototype and a cast, which is how it has always
// arrived here - the header changes where the prototype comes from and nothing
// about what the compiler does with it.
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *a;
    int i;
    int sum;

    a = (int *)malloc(10 * sizeof(int));
    i = 0;
    while (i < 10) { a[i] = i * i; i = i + 1; }

    sum = 0;
    i = 0;
    while (i < 10) { sum = sum + a[i]; i = i + 1; }
    free(a);

    printf("%d %d %ld %d\n", sum, abs(-7), labs(-100000L), atoi("123"));
    return 0;
}
