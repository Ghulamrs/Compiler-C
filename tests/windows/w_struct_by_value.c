// expect: 0
// Aggregates by value under Microsoft x64, which is the rule this ABI departs
// from System V on most sharply.
//
// System V cuts an aggregate into eightbytes and classifies each, so a 16-byte
// struct of two doubles travels in two vector registers. Microsoft does none of
// that. An aggregate goes in a register only when its size is exactly 1, 2, 4
// or 8 - the sizes a register holds whole - and never in a vector register.
// Every other size, 3 and 5 and 6 and 7 as much as anything over 8, is copied
// by the caller and passed as a pointer to that copy.
//
// The copy is the part worth checking, and mutate() below is why: the callee is
// entitled to write through the pointer it was handed, so a caller that passed
// the original object rather than a copy would have it modified underneath.
struct P  { int x; int y; };            /* 8  - a register                */
struct S  { char c; short s; };         /* 4  - a register                */
struct B  { int a; int b; int c; };     /* 12 - not a register size       */
struct V  { double a; double b; };      /* 16 - System V would use xmm    */
struct Big{ int a[8]; };                /* 32 - by pointer, returned so   */
union  U  { int i; unsigned u; };       /* 4  - a register                */

int sump(struct P p)   { return p.x + p.y; }
int sums(struct S v)   { return v.c + v.s; }
int sumb(struct B v)   { return v.a + v.b + v.c; }
int sumu(union U u)    { return u.i; }
double sumv(struct V v){ return v.a + v.b; }

/* writes to its own parameter, which must not reach the caller's object */
int mutate(struct B v) { v.a = 99; v.b = 99; v.c = 99; return v.a; }

struct P mkp(int x, int y)  { struct P p; p.x = x; p.y = y; return p; }
struct B mkb(int a)         { struct B v; v.a = a; v.b = a+1; v.c = a+2; return v; }
struct Big fill(int base)   { struct Big b; int i = 0;
                              while (i < 8) { b.a[i] = base + i; i = i + 1; }
                              return b; }
int sumbig(struct Big b)    { int t = 0; int i = 0;
                              while (i < 8) { t = t + b.a[i]; i = i + 1; }
                              return t; }

/* an aggregate after the four register slots are spent, so it lands on the
   stack as a pointer rather than in a register */
int tail(int a, int b, int c, struct B v, int d)
{
    return a + b + c + v.a + v.b + v.c + d;
}

int main(void)
{
    struct P p; struct S s; struct B b; struct V v; union U u;
    struct Big g;
    int bad;

    bad = 0;

    p.x = 3; p.y = 4;
    if (sump(p) != 7) bad = bad + 1;

    s.c = 65; s.s = 300;
    if (sums(s) != 365) bad = bad + 2;

    b.a = 1; b.b = 2; b.c = 3;
    if (sumb(b) != 6) bad = bad + 4;

    u.i = 42;
    if (sumu(u) != 42) bad = bad + 8;

    v.a = 1.5; v.b = 2.25;
    if (sumv(v) != 3.75) bad = bad + 16;

    /* the callee scribbles on its copy; ours must be untouched */
    if (mutate(b) != 99) bad = bad + 32;
    if (b.a != 1 || b.b != 2 || b.c != 3) bad = bad + 64;

    p = mkp(10, 20);
    if (p.x != 10 || p.y != 20) bad = bad + 128;

    b = mkb(5);
    if (b.a != 5 || b.b != 6 || b.c != 7) bad = bad + 256;

    g = fill(100);
    if (g.a[0] != 100 || g.a[7] != 107) bad = bad + 512;
    if (sumbig(g) != 828) bad = bad + 1024;   /* 100..107 */

    if (tail(1, 2, 3, b, 4) != 28) bad = bad + 2048;  /* 1+2+3+5+6+7+4 */

    return bad;
}
