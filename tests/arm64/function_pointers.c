// expect: 0
// Calling through a pointer rather than a name. Judged against clang, because
// what matters is not the arithmetic but that sp and the argument registers
// are where the callee expects them at the branch.
//
// The variadic case below is the one that constrains the implementation. The
// address to branch to has to survive the argument marshalling, so it lives on
// the stack rather than in a register - and the variadic part lives on the
// stack too, at sp on entry. Pushing the address before opening the variadic
// area rather than after leaves it underneath at the branch, and every
// variadic argument is then eight bytes from where the callee looks.
#include <stdio.h>

struct Big { long a, b, c, d; };

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
static int mul(int a, int b) { return a * b; }

static double mix(double a, int b, double c) { return a * b + c; }

// An aggregate too large for registers, so the caller copies it and passes a
// pointer - and the result comes back through x8. Both of those have to still
// be right when the branch target is computed.
static struct Big scale(struct Big s, int k)
{
    struct Big r;
    r.a = s.a * k; r.b = s.b * k; r.c = s.c * k; r.d = s.d * k;
    return r;
}

struct Ops { int (*f)(int, int); int tag; };

int main(void)
{
    int (*table[3])(int, int);
    int (*p)(int, int);
    double (*q)(double, int, double);
    struct Big (*r)(struct Big, int);
    int (*v)(const char *, ...) = printf;
    struct Ops o;
    struct Big g, got;
    int i, s = 0;

    table[0] = add; table[1] = sub; table[2] = mul;
    for (i = 0; i < 3; i++) s += table[i](10, 3);   /* 13 + 7 + 30 = 50 */

    p = add;
    q = mix;
    r = scale;
    o.f = sub; o.tag = 4;

    g.a = 1; g.b = 2; g.c = 3; g.d = 4;
    got = r(g, 3);                                  /* 3 6 9 12 = 30 */

    /* "9 x 2.5 1 2 3\n" is fourteen characters */
    i = v("%d %s %g %d %d %d\n", 9, "x", 2.5, 1, 2, 3);

    return (s - 50)
         + (p(20, 22) - 42)
         + (int)(q(2.5, 4, 1.25) - 11.25)
         + (int)((got.a + got.b + got.c + got.d) - 30)
         + (o.f(10, 6) - 4)
         + (i - 14);
}
