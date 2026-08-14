// expect: 0
// windows-only: it calls printf, and a Windows-convention call into glibc's
//   System V printf is the one boundary this cannot cross on Linux
// Microsoft x64 has no %al convention and no prototype at the callee, so it
// sends a variadic float in both files at once: the vector register and the
// integer register of the same slot. printf reads the integer twin, so putting
// the bits only in %xmm2 prints garbage for %f and everything else correctly.
//
// The double is the third argument here deliberately. Slot two is %r8 and
// %xmm2, so a backend still counting the two files independently would reach
// for %xmm0 and be wrong about the register as well as about the pairing.
#include <stdio.h>

int main(void)
{
    printf("int %d, double %f, string %s\n", 42, 3.14159, "world");
    return 0;
}
