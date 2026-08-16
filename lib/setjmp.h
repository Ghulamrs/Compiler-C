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
// **Windows works now too, and what this header used to blame was wrong.** It
// said the obstacle was unwind data. cc1 emits .pdata and .xdata since - see
// Masm.cpp - and that turned out not to be the thing. Three facts, each
// measured on the Windows host rather than reasoned about:
//
// 1. **'_setjmp' takes a hidden second argument.** The UCRT declares it with
//    one parameter and MSVC ignores its own prototype, because '_setjmp' is on
//    the intrinsic list: disassembling cl's output shows 'mov rdx, rsp' before
//    'call _setjmp'. Calling it with one argument left rdx holding rubbish,
//    and longjmp then unwound toward a frame that never existed -
//    STATUS_BAD_FUNCTION_TABLE, and the process gone.
//
// 2. **A zero frame is the right thing to pass, and needs no unwind data.**
//    longjmp then restores the context instead of unwinding, which is all
//    longjmp means in C90: no destructors, no termination handlers. Checked
//    with the unwind data present and again with it stripped out of the
//    assembly by hand - identical, correct behaviour both times. So the unwind
//    data earns its place on the ABI and on stack walking, not here.
//
// 3. **The last blocker was alignment.** jmp_buf is an array of a sixteen-byte
//    aligned type under the UCRT, because '_setjmp' fills it with aligned xmm
//    saves. A file-scope buffer came out aligned by luck and worked; a local
//    one landed on an odd eightbyte and faulted on the first save. cc1 now
//    gives any object of sixteen bytes or more sixteen-byte alignment -
//    objectAlign in Type.cpp - which costs at most eight bytes of frame and
//    needs no syntax the language does not have.
#ifndef _CC1_SETJMP_H
#define _CC1_SETJMP_H

// The size is the platform's and not this compiler's: setjmp lives in the C
// library and writes as many bytes as it was built to write, so jmp_buf has to
// be at least as large or the call scribbles past the end of the object.
// Measured on each rather than assumed.
//
//     macOS arm64     192 bytes    24 longs
//     Linux x86-64    200 bytes    25 longs
//     Windows x64     256 bytes    32 long longs
//
// 'long' rather than 'int' for the element, because every one of these
// libraries saves callee-saved registers and a stack pointer into it, and
// arm64 saves d8-d15 as well - all of which want eight-byte alignment, which
// an array of long has and an array of int does not. 'long long' on Windows,
// where long is four bytes.
//
// Windows wants more than eight. Its jmp_buf is an array of a sixteen-byte
// aligned type, because '_setjmp' fills it with aligned xmm saves and takes an
// access violation on the first of them if the buffer sits on an odd
// eightbyte. Nothing here says so, and nothing needs to: cc1 gives every
// object of sixteen bytes or more sixteen-byte alignment, which is what
// objectAlign in Type.cpp is for and why a local jmp_buf is safe.
#if defined(_WIN32)
typedef long long jmp_buf[32];
#elif defined(__APPLE__)
typedef long jmp_buf[24];
#else
typedef long jmp_buf[25];
#endif

// C90 7.6.2.1 says setjmp is a macro. On the two Unixes it is a plain
// declaration here, which is a deviation worth naming: a macro would only have
// to expand to this call, and the reason the standard allows one - that the
// implementation may need to capture something the caller cannot pass - does
// not arise for either library.
//
// **On Windows that reason arises exactly.** The UCRT declares '_setjmp' with
// one parameter and MSVC ignores its own prototype, because '_setjmp' is on
// the intrinsic list: cl emits 'mov rdx, rsp' before the call, handing it a
// frame the prototype never mentions. A compiler that takes the header at its
// word calls it with one argument, leaves rdx holding whatever was there, and
// longjmp then unwinds toward a frame that never existed - which is
// STATUS_BAD_FUNCTION_TABLE, not a wrong answer.
//
// Zero is passed for it deliberately. A null frame tells longjmp to restore
// the context rather than unwind, and for C90 that is the whole of what
// longjmp means: there are no destructors and no termination handlers for an
// unwind to run. It is also the reading that does not depend on the unwind
// data being perfect - though cc1 emits that now too, and this was checked
// both with it and with it stripped back out.
//
// C90 7.6.2.1 also restricts where setjmp may appear to four contexts, and
// 'r = setjmp(env)' is not among them. Every real program writes it, so it
// works here; the restriction is what the standard permits an implementation
// to rely on, not a promise a program must keep to be compiled.
#if defined(_WIN32)
int _setjmp(jmp_buf env, void *frame);
#define setjmp(env) _setjmp((env), (void *)0)
#else
int setjmp(jmp_buf env);
#endif

void longjmp(jmp_buf env, int val);

#endif
