// expect: 0
// A quoted include falls back to the search path when nothing sits beside the
// file, which is C's rule and the reason "stdio.h" finds the shipped header
// from a directory that has no stdio.h in it. The angle form would not look
// beside the file at all; that difference is the whole point of having two.
#include "stdio.h"

int main(void) {
    puts("fallback");
    return 0;
}
