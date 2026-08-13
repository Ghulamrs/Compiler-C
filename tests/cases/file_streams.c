// expect: 0
// The three standard streams, which are extern FILE * rather than functions -
// so this is the case that checks an object declared in <stdio.h> and defined
// inside libc resolves at link time, not just that a prototype does.
#include <stdio.h>

int main(void) {
    int n;

    n = fprintf(stdout, "to stdout: %d\n", 1);
    printf("returned %d\n", n);

    fputs("also stdout\n", stdout);
    fflush(stdout);

    // stderr is written but not compared: the suite captures stdout only, and
    // that is the point of writing to it here - it must not appear above.
    fputs("this goes to stderr\n", stderr);
    fflush(stderr);

    printf("stdin is not null: %d\n", stdin != 0);
    return 0;
}
