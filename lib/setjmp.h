// setjmp.h - a non-local jump.
//
// This header used to be an '#error' with an essay attached, because the
// compiler could not generate code that survived a longjmp. The obstacle was
// never in the header. It was in how an assignment was compiled:
//
//     genAddr(target); push; value(); pop
//
// - the destination address computed first and parked on the stack while the
// value was worked out. setjmp returns twice, and the second return arrives
// with the stack pointer restored to what setjmp recorded, so those pushed
// bytes had long since been freed and handed to some other call. 'r =
// setjmp(env)' then stored the result through whatever now lived there. Not a
// wrong value - a wild pointer.
//
// Both code generators now evaluate the value first and take the destination
// address afterwards, so nothing of the compiler's sits below the stack
// pointer across the call. C90 leaves the order of an assignment's operands
// unspecified, which is what makes that a choice rather than a liberty. The
// fix is in X86_64Linux.cpp and Arm64Darwin.cpp, and it is general: it is
// simply invisible anywhere except here, because nothing else returns twice.
//
// **Windows is still refused, and the reason written here before was wrong.**
// It said the blocker was unwind data. cc1 emits .pdata and .xdata now - see
// Masm.cpp - and that turned out not to be what stopped this working. Three
// things were measured on the Windows host rather than reasoned about, and
// they are recorded here so the next attempt starts from facts:
//
// 1. **'_setjmp' really does take a hidden second argument**, which is the one
//    part of the old note that was right. The UCRT header declares it with one
//    parameter and MSVC ignores its own prototype, because '_setjmp' is on the
//    intrinsic list: disassembling cl's output shows 'mov rdx, rsp' before
//    'call _setjmp'. cc1 called it with one argument and left rdx holding
//    whatever was there, so longjmp unwound toward a frame that never existed
//    and the process died with STATUS_BAD_FUNCTION_TABLE.
//
// 2. **Passing a zero frame works, and needs no unwind data at all.** With
//    '_setjmp(env, 0)' longjmp restores the context instead of unwinding,
//    which is the right semantics for C90 - there are no destructors and no
//    termination handlers for an unwind to run. Verified with the unwind data
//    present and again with it stripped out: identical, correct behaviour
//    both times. So the unwind data is worth having for the ABI, for
//    debuggers and for stack walking, but it is not what this header needs.
//
// 3. **The blocker is alignment.** jmp_buf is 'SETJMP_FLOAT128[16]' under the
//    UCRT, sixteen-byte aligned because '_setjmp' saves xmm6-xmm15 into it
//    with aligned moves. cc1's widest alignment is eight. A file-scope buffer
//    happens to come out aligned and works; a local one lands on an odd
//    eightbyte and '_setjmp' takes an access violation on the first xmm save.
//    Shipping a header that works at file scope and faults in a function is
//    the same trade this file refused to make before, so it still refuses.
//
// Closing it needs one of two things: an alignment the language can express -
// cc1 has no '_Alignas' and no '__declspec(align)' - or a jmp_buf deliberately
// over-sized with the macros rounding the pointer up inside it, which works
// but makes longjmp a macro and the type a size the platform never named.
#ifndef _CC1_SETJMP_H
#define _CC1_SETJMP_H

#ifdef _WIN32
#error <setjmp.h> is not supported for x86_64-windows - the UCRT wants a 16-byte aligned jmp_buf and this compiler cannot align anything past 8, so a local one faults inside _setjmp; see the note at the top of this header
#endif

// The size is the platform's and not this compiler's: setjmp lives in the C
// library and writes as many bytes as it was built to write, so jmp_buf has to
// be at least as large or the call scribbles past the end of the object.
// Measured on each rather than assumed.
//
//     macOS arm64    192 bytes    24 longs
//     Linux x86-64   200 bytes    25 longs
//
// 'long' rather than 'int' for the element, because both libraries save
// callee-saved registers and a stack pointer into this, and arm64 saves d8-d15
// as well - all of which want eight-byte alignment, which an array of long has
// and an array of int does not.
#if defined(__APPLE__)
typedef long jmp_buf[24];
#else
typedef long jmp_buf[25];
#endif

// C90 7.6.2.1 says setjmp is a macro. It is a plain declaration here, which is
// a deviation worth naming: a macro would have to expand to this call anyway,
// and the reason the standard allows the implementation to make it one - that
// it may need to capture something the caller cannot pass - does not arise for
// either of these two libraries.
//
// The same clause restricts where setjmp may appear to four contexts, and
// 'r = setjmp(env)' is not among them. Every real program writes it, so it
// works here; the restriction is what the standard permits an implementation
// to rely on, not a promise the program must keep to be compiled.
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif
