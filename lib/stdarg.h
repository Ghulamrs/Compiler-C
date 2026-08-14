#ifndef _STDARG_H
#define _STDARG_H

/* System V x86-64, and deliberately not a portable header.
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
 * x86_64-windows makes a va_list a plain char *, because its callee spills into
 * the shadow space the caller already left; arm64-darwin does the same because
 * Apple puts the variadic part on the stack to begin with. Neither is served by
 * this file, and both refuse a variadic definition by name until it is.
 */
typedef struct {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} __va_list_tag;

typedef __va_list_tag va_list[1];

/* The second argument is accepted and ignored. Which parameters were named is
 * a property of the definition, and the compiler already knows it; asking the
 * caller to name the last one again is a convention from the days when this
 * was written in C rather than known to the front end. */
#define va_start(ap, last) __builtin_va_start(ap)
#define va_end(ap)         ((void)0)

/* The three <stdio.h> functions that take a va_list. They live here and not
 * there because that is where va_list is: a file that never asked for stdarg.h
 * has no use for them and should not have the name in scope. */
int vprintf(const char *, va_list);
int vfprintf(void *, const char *, va_list);
int vsprintf(char *, const char *, va_list);

#endif
