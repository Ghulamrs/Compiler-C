#ifndef _STDARG_H
#define _STDARG_H

/* Two ABIs, two va_lists, and the difference is not cosmetic.
 *
 * va_list is the ABI's, not this compiler's choice: vprintf lives in the C
 * library and reads whatever the platform says a va_list is, so the layout
 * below has to match it byte for byte or the first forwarded call walks the
 * wrong memory. Under System V that is a four-field record - two offsets into
 * the register save area the callee spilled to, and two pointers - and it is
 * declared as an array of one so that passing it decays to a pointer and the
 * callee's walk is visible to the caller, which is what the standard requires
 * of va_list without saying how.
 *
 * Under Microsoft x64 it is a plain char *. Every argument, named or not, sits
 * in a consecutive eight-byte slot starting at the shadow space the caller
 * already left, so walking them is pointer arithmetic and nothing else - and a
 * variadic float arrives in the integer register as well as the vector one,
 * which is what makes reading eight bytes enough for a double too.
 *
 * __builtin_va_start takes a pointer to the va_list object either way. System V
 * gets one for free, an array of one decaying; Windows has to take the address,
 * which is the only reason the two macros below differ in shape.
 */
#ifdef _WIN32

typedef char *va_list;
#define va_start(ap, last) __builtin_va_start(&(ap))
#define va_arg(ap, type)   __builtin_va_arg(&(ap), type)

#else

typedef struct {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} __va_list_tag;

typedef __va_list_tag va_list[1];
#define va_start(ap, last) __builtin_va_start(ap)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)

#endif

/* The second argument to va_start above is accepted and ignored. Which
 * parameters were named is a property of the definition and the compiler
 * already knows it; asking the caller to name the last one again is a
 * convention from when this was written in C rather than known to the front
 * end. */
#define va_end(ap)         ((void)0)

/* The three <stdio.h> functions that take a va_list. They live here and not
 * there because that is where va_list is: a file that never asked for stdarg.h
 * has no use for them and should not have the name in scope. */
int vprintf(const char *, va_list);
int vfprintf(void *, const char *, va_list);
int vsprintf(char *, const char *, va_list);

#endif
