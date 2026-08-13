// expect: 0
// Two spellings of one name, in one file, meaning two different files.
//
// "string.h" is the one sitting in this directory. <string.h> must not be:
// the angle form goes to the search path and never looks beside the including
// file, so it finds the header cc1 ships - and finds glibc's when gcc builds
// this same directory for the comparison. If <...> ever started looking beside
// the file first, strlen would go undeclared and this case would stop
// compiling, which is the only way the difference between the two forms can be
// asserted rather than described.
#include "string.h"
#include <string.h>
#include <stdio.h>

int main(void) {
    printf("%d %lu\n", local_answer(), strlen("abcd"));
    return 0;
}
