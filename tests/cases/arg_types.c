// expect: 0
// The boundary of the argument type check, from the accepting side. The suite
// holds programs the compiler must compile, so what it can pin is the set of
// conversions that must NOT be refused - an over-eager check would fail here
// rather than in whatever real program met it first.
//
// The refusals themselves are verified by hand against deliberately wrong
// files, and their messages are quoted in docs/STATUS.md.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int takes_int(int n)          { return n; }
int takes_long(long n)        { return (int)n; }
int takes_char(char c)        { return c; }
int takes_double(double d)    { return (int)(d * 2); }
int takes_ptr(char *p)        { return p == 0; }
int takes_const_ptr(const char *p) { return p == 0; }
int takes_void_ptr(void *p)   { return p == 0; }

int main(void) {
    char buf[8];
    char *p;
    void *v;

    // Arithmetic to arithmetic, in every direction. Each is a defined
    // conversion, so each must pass the check and then be converted.
    printf("%d %d %d %d\n", takes_int(1), takes_long(2), takes_char(3),
           takes_double(1.5));
    printf("%d %d\n", takes_int('a'), takes_char(300));
    printf("%d\n", takes_double(7));

    // The null pointer constant reaches a pointer, spelled both ways.
    printf("%d %d\n", takes_ptr(0), takes_ptr(NULL));

    // void * in either direction, which is what malloc and free run on.
    v = malloc(8);
    printf("%d %d\n", takes_ptr((char *)v), takes_void_ptr(v));
    free(v);

    // An array decays to a pointer at the call, and a string literal is an
    // array like any other.
    strcpy(buf, "abc");
    printf("%d %d %lu\n", takes_ptr(buf), takes_const_ptr("literal"),
           strlen(buf));

    // The same type reached by two routes is one interned type.
    p = buf;
    printf("%d\n", takes_ptr(p));

    // Past a variadic's named parameters there is no parameter to check
    // against, so a pointer and an int sit side by side here quite legally.
    printf("%s %d %p\n", "past", 1, (void *)0);
    return 0;
}
