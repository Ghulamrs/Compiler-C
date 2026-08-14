// expect: 0
// windows-only: it calls the C library, which the Linux hosting trick cannot
//   survive - a Windows-convention call into glibc's printf segfaults
// printf, sprintf and fprintf agreeing on Windows, which takes two things this
// suite did not need before.
//
// The convention: the double travels in %xmm and in the integer register of
// the same slot, because a variadic callee here has no prototype to tell it
// which file to read. All three functions are variadic, so all three depend on
// it, and the count each returns is the check that they agree.
//
// The library: sprintf is not a symbol on Windows. The UCRT keeps printf,
// fprintf, puts and fputs as real exports and makes sprintf, the v-family and
// the scanf family inline wrappers over __stdio_common_* in its own header.
// Declaring them as the ordinary functions C says they are is correct, and it
// is why the runner links legacy_stdio_definitions.
#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[128];
    int a, b, c;
    FILE *out = fopen("w_stdio_family.tmp", "w");
    if (out == 0) return 9;

    a = printf( "int %d, double %f, string %s, char %c\n", 42, 3.14159, "world", 'Z');
    b = sprintf(buf, "int %d, double %f, string %s, char %c\n", 42, 3.14159, "world", 'Z');
    c = fprintf(out, "int %d, double %f, string %s, char %c\n", 42, 3.14159, "world", 'Z');
    fclose(out);
    remove("w_stdio_family.tmp");

    if (a != b) return 1;
    if (a != c) return 2;
    if ((int)strlen(buf) != a) return 3;
    return 0;
}
