// expect: 0
// 'typedef int F(void);' names a function type. The point of writing one is
// that 'F *p' then reads left to right, where the equivalent 'int (*p)(void)'
// buries the name in the middle of its own type.
//
// Two things had to change for this. The declarator leaves a parameter list
// alone outside parentheses, because those tokens are how a function
// definition is told from an object declaration - a typedef can have no body,
// so there is nothing ambiguous and the list is read straight away. And
// 'typedef' was missing from the list of words that begin a declaration, which
// did not make a block-scope typedef an error but an *expression*.
#include <stdarg.h>

static int one(void)         { return 1; }
static int add(int a, int b) { return a + b; }
static int calls = 0;
static void bump(void)       { calls++; }

static int sum(const char *fmt, ...)
{
    va_list ap;
    int t = 0;
    (void)fmt;
    va_start(ap, fmt);
    t += va_arg(ap, int);
    t += va_arg(ap, int);
    va_end(ap);
    return t;
}

typedef int  F(void);               // the plain case
typedef int  G(int, int);           // with parameters
typedef void V(void);               // returning nothing
typedef int  VA(const char *, ...); // variadic
typedef int (P)(void);              // the parenthesised spelling
typedef int (*PF)(void);            // the pointer form, which already worked

// A function declared *by* the typedef. C90 allows that as a declaration and
// forbids it as a definition - a body would have no names for the parameters.
F one_again;

static int call(F *cb) { return cb(); }   // as a parameter type

int main(void)
{
    typedef int Local(void);        // in a block, which used to be unreachable
    typedef int Plain;              // and the ordinary kind beside it

    F  *p  = one;
    G  *q  = add;
    V  *v  = bump;
    VA *va = sum;
    P  *pp = one;
    PF  r  = one;
    Local *l = one;
    Plain  n = 5;
    F  *table[2];

    table[0] = one;
    table[1] = one_again;
    v();

    return (p() != 1)
         + (q(20, 22) != 42)
         + (calls != 1)
         + (va("", 1, 2) != 3)
         + (pp() != 1)
         + (r() != 1)
         + (l() != 1)
         + (n != 5)
         + (table[0]() != 1)
         + (table[1]() != 1)
         + (call(one) != 1);
}

int one_again(void) { return 1; }
