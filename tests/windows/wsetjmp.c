// expect: 0
// windows-only: the whole case is the UCRT's setjmp contract, and glibc has a
// different one - '_setjmp' there takes one argument and longjmp never had a
// frame to be told about. Running this on Linux would not be a weaker check,
// it would be a check of something else.
//
// <setjmp.h> on the target that refused it longest, assembled by ml64 and
// linked by link.exe against the real UCRT. Three things have to be right at
// once for this to return 0, and each was wrong on its own first:
//
//   - the hidden second argument to '_setjmp'. The UCRT declares it with one
//     parameter and MSVC ignores that, passing the frame in rdx because
//     '_setjmp' is an intrinsic. Calling it with one argument leaves rubbish
//     there and longjmp dies with STATUS_BAD_FUNCTION_TABLE.
//   - the alignment of jmp_buf. The library fills it with aligned xmm saves,
//     so an odd eightbyte is an access violation on the first of them. 'local'
//     below is the one that catches it: a file-scope buffer comes out aligned
//     whatever the compiler does, and a local one only if the compiler means
//     it to.
//   - unwind data, which is not needed for this and is emitted anyway. Passing
//     a null frame tells longjmp to restore rather than unwind, which is all
//     longjmp is in C90. This case passes with the .pdata stripped out too;
//     what would fail without it is a debugger trying to walk the stack.
//
// 'shove' sits in front of the local buffer on purpose, so the frame is not
// accidentally arranged to make it aligned.
#include <stdio.h>
#include <setjmp.h>

static jmp_buf genv;
static int depth = 0;

static void deep(int n) {
    char pad[96];
    int i;
    for (i = 0; i < 96; i++) pad[i] = (char)(n + i);
    depth = depth + (pad[0] == (char)n);
    if (n > 0) deep(n - 1);
    longjmp(genv, 42);
}

int main(void) {
    char shove;
    jmp_buf local;
    int r;
    volatile int witness = 1234;

    shove = 7;
    if (((unsigned long long)(void *)genv) % 16 != 0) return 10;
    if (((unsigned long long)(void *)local) % 16 != 0) return 11;

    // The assignment form, across four frames of scribbled-on stack.
    r = setjmp(genv);
    if (r == 0) { deep(3); return 12; }
    if (r != 42 || witness != 1234) return 13;

    // A buffer in the frame rather than at file scope.
    if (setjmp(local) == 0) longjmp(local, 7);
    else if (shove != 7) return 14;

    // C90 7.6.2.1: longjmp with zero makes setjmp return 1.
    if (setjmp(local) == 0) longjmp(local, 0);

    printf("depth=%d\n", depth);
    return depth == 4 ? 0 : 15;
}
