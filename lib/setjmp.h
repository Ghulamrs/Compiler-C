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
// **Windows is still refused, and for a different reason that has not moved.**
// The UCRT has no one-argument setjmp. Its '_setjmp(env, frame)' takes the SEH
// frame pointer, and longjmp unwinds through the .pdata and .xdata tables that
// describe every function on the way back - which cc1 does not emit. Declared
// by hand, '_setjmp(env, 0)' does return and longjmp does arrive back at it,
// but the return value is lost and the program loops until the stack is gone.
// Unwind data is a real piece of work and is not done, so this says so.
#ifndef _CC1_SETJMP_H
#define _CC1_SETJMP_H

#ifdef _WIN32
#error <setjmp.h> is not supported for x86_64-windows - the UCRT's setjmp takes an SEH frame pointer and longjmp unwinds through .pdata and .xdata, which this compiler does not emit; see the note at the top of this header
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
