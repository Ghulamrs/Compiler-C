// expect: 0
// windows-only: it calls the C library, which the Linux hosting trick cannot
//   survive - a Windows-convention call into glibc's printf segfaults
// A variadic function written in this language, on Microsoft x64, where the
// whole mechanism is different from System V's and much smaller.
//
// There is no register save area here and no %al. Every argument, named or
// not, owns a consecutive eight-byte slot from 16(%rbp) up, and the first four
// of those are the shadow space the caller already left - so the callee spills
// %rcx %rdx %r8 %r9 into somewhere that already exists, and va_list is one
// pointer at the slot after the named ones.
//
// The double is the check that this is enough. It arrives in %xmm2 and in %r8,
// so spilling the integer registers captures it too, and vprintf reading eight
// bytes gets the right value - which is only true because of the both-files
// rule, and would print rubbish without it.
#include <stdio.h>
#include <stdarg.h>

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
    int w = my_printf("int %d, double %f, string %s\n", 42, 3.14159, "world");
    if (w != 38) return 1;
    return 0;
}
