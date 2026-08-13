// expect: 0
// Structs passed and returned by value, in the five shapes System V classifies
// differently. gcc builds the same file, so the classification is checked
// against the ABI's own implementation rather than against my reading of it -
// which matters here more than anywhere else, because getting a lane wrong
// produces a program that runs and lies.
#include <stdio.h>

struct P { int x; int y; };          /* 8 bytes, one INTEGER eightbyte     */
struct V { double a; double b; };    /* 16 bytes, two SSE eightbytes       */
struct M { int n; double d; };       /* 16 bytes, INTEGER then SSE         */
struct S { char c; short s; };       /* 4 bytes, a partly filled eightbyte */
struct B { int a; int b; int c; };   /* 12 bytes, one full and one half    */

static struct P mkp(int x, int y) { struct P p; p.x = x; p.y = y; return p; }
static int sump(struct P p) { return p.x + p.y; }

static struct V mkv(double a, double b) { struct V v; v.a = a; v.b = b; return v; }
static double sumv(struct V v) { return v.a + v.b; }

static struct M mkm(int n, double d) { struct M m; m.n = n; m.d = d; return m; }
static double sumn(struct M m) { return m.n + m.d; }

static struct S mks(char c, short s) { struct S v; v.c = c; v.s = s; return v; }
static int sums(struct S v) { return v.c + v.s; }

static struct B mkb(int a, int b, int c) { struct B v; v.a = a; v.b = b; v.c = c; return v; }
static int sumb(struct B v) { return v.a + v.b + v.c; }

// Taken by value and returned by value, with a call in between - which is the
// case that needs the result slot to be a real place rather than a register.
static struct P twice(struct P p) { return mkp(p.x * 2, p.y * 2); }

int main(void) {
    struct P p = mkp(3, 4);          /* initialised from a returned struct */
    struct V v = mkv(1.5, 2.25);
    struct M m = mkm(7, 0.5);
    struct S s = mks(65, 300);
    struct B b = mkb(1, 2, 3);
    struct P q;

    printf("P : %d %d -> %d\n", p.x, p.y, sump(p));
    printf("V : %.2f %.2f -> %.2f\n", v.a, v.b, sumv(v));
    printf("M : %d %.1f -> %.1f\n", m.n, m.d, sumn(m));
    printf("S : %d %d -> %d\n", s.c, s.s, sums(s));
    printf("B : %d %d %d -> %d\n", b.a, b.b, b.c, sumb(b));

    q = twice(p);
    printf("Q : %d %d\n", q.x, q.y);
    printf("chain: %d\n", sump(twice(mkp(5, 6))));
    printf("mixed: %d\n", sump(p) + (int)sumv(v));

    q = p;                            /* the plain copy still works */
    printf("copy : %d %d\n", q.x, q.y);
    return 0;
}
