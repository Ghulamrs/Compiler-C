// expect: 0
// A variadic function written in this language, forwarding to the platform's
// vprintf. The whole point is that our va_list has to be the ABI's and not our
// own invention: vprintf lives in the C library and will read whatever System V
// says a va_list is, so a layout that merely satisfies this compiler walks the
// wrong memory on the first call.
//
// The double matters. It is what makes the caller set %al non-zero, which is
// what makes the callee's prologue spill the vector registers at all - so a
// prologue that saved only the integer half still prints the strings and the
// integers correctly and gets this line wrong.
#include <stdarg.h>
int vprintf(const char *, va_list);
int printf(const char *, ...);

int my_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int main(void)
{
    int written = my_printf("%d %s %f %d\n", 7, "mid", 2.5, 9);
    printf("wrote %d\n", written);
    return 0;
}
