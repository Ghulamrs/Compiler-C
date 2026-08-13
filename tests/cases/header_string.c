// expect: 0
// <string.h>, which also reaches <stddef.h> for size_t - an angle include made
// from inside a file that was itself found on the search path.
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[32];
    char dst[32];
    size_t n;

    strcpy(buf, "hello");
    n = strlen(buf);
    printf("%s %lu\n", buf, n);

    strcat(buf, " there");
    printf("%s %d\n", buf, strcmp(buf, "hello there"));

    memset(dst, 0, 32);
    memcpy(dst, buf, strlen(buf) + 1);
    printf("%s %d\n", dst, memcmp(dst, buf, 6));

    printf("%d %d\n", strncmp("abc", "abd", 2), strncmp("abc", "abd", 3) < 0);
    return 0;
}
