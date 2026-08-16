// setjmp.h - a non-local jump, and the one header here that is refused.
//
// This file exists to say no with a reason, because the reason is a real
// limitation of the compiler rather than a missing declaration. The types and
// sizes below are right, and were measured on all three platforms; what is
// missing is that cc1 cannot generate code that survives a longjmp.
//
// **The assignment form corrupts memory.** `r = setjmp(env)` is the idiom
// every program uses, and it compiles to
//
//     str x0, [sp, #-16]!     ; the address of r, pushed before the call
//     bl  _setjmp
//     ldr x1, [sp], #16       ; popped after it
//     str w0, [x1]
//
// The address of the destination lives on the stack across the call. On the
// first pass that is fine. But longjmp restores sp to the value setjmp
// recorded, and by then the sixteen bytes at that address have been popped,
// freed and reused by whatever the program did in between - so the pop reads
// somebody else's data and the store writes the setjmp result through it. Not
// a wrong value: a wild pointer.
//
// C90 4.6.2.1 does say that a non-volatile local changed between setjmp and
// longjmp has an indeterminate value afterwards, so `r` holding rubbish would
// be permitted. Writing through rubbish is not.
//
// The comparison form - `if (setjmp(env) == 0)` - happens to survive, because
// nothing is stored and no address is held. Shipping a header that works for
// one spelling of the idiom and corrupts the heap for the other is worse than
// shipping neither.
//
// Windows fails a second way on top of that: the UCRT has no one-argument
// setjmp at all. Its `_setjmp(env, frame)` takes the SEH frame pointer, and
// longjmp unwinds through the .pdata and .xdata tables that describe every
// function on the way back - and cc1 emits none of those, which `grep -c
// pdata` over anything it writes for Windows will confirm. Declared by hand,
// `_setjmp(env, 0)` does return and longjmp does arrive back at it, but with
// the return value lost, so the program loops until the stack is gone.
//
// **What it would take.** The destination address of an assignment must not
// live on the stack across the call that produces the value - recomputing it
// after the call, as the initialiser walkers already rebuild an lvalue from
// its name rather than cloning it, would fix this everywhere and not only
// here. Windows needs unwind data as well, which is a larger job.
//
// The sizes, which were measured and are correct, for whoever writes that:
//
//     macOS arm64   192 bytes    long jmp_buf[24]
//     Linux x86-64  200 bytes    long jmp_buf[25]
//     Windows x64   256 bytes    long long jmp_buf[32], and _setjmp(env, frame)
#ifndef _CC1_SETJMP_H
#define _CC1_SETJMP_H

#error <setjmp.h> is not supported - a longjmp back into an assignment writes the result through a stack slot that has since been reused; see the note at the top of this header

#endif
