// expect: 0
// Returning a struct too large for the registers. The caller hands the callee a
// pointer to somewhere it owns, in %rdi, before any real argument is placed -
// so every integer argument shifts along by one, and a function with six of
// them now spills its sixth to the stack that used to fit them all.
//
// That shift is the whole risk here, and it is why gcc building this file
// matters more than the printed numbers: disagree with it about which register
// holds what and the program still runs.
#include <stdio.h>

struct Big  { int a[8]; };                  /* 32 bytes */
struct Wide { double x, y, z; };            /* 24 bytes, and floating */
struct Mix  { int n; double d; char s[8]; };/* 24 bytes, mixed */
struct Small{ int p, q; };                  /* 8 bytes - still in a register */

struct Big fill(int base) {
    struct Big b;
    int i = 0;
    while (i < 8) { b.a[i] = base + i; i = i + 1; }
    return b;
}

struct Wide scale(double k) {
    struct Wide w;
    w.x = k;
    w.y = k * 2.0;
    w.z = k * 3.0;
    return w;
}

// Six integer parameters and a hidden pointer. Five of the six fit in what is
// left of the register file; the sixth goes to memory.
struct Big six(int a, int b, int c, int d, int e, int f) {
    struct Big r;
    r.a[0] = a; r.a[1] = b; r.a[2] = c;
    r.a[3] = d; r.a[4] = e; r.a[5] = f;
    r.a[6] = a + f;
    r.a[7] = 99;
    return r;
}

struct Mix make(int n, double d, char c) {
    struct Mix m;
    int i = 0;
    m.n = n;
    m.d = d;
    while (i < 7) { m.s[i] = c; i = i + 1; }
    m.s[7] = 0;
    return m;
}

// A memory struct in and a memory struct out, at once.
struct Big twice(struct Big in) {
    struct Big out;
    int i = 0;
    while (i < 8) { out.a[i] = in.a[i] * 2; i = i + 1; }
    return out;
}

// A register struct in, a memory struct out - the two paths meeting.
struct Wide spread(struct Small s) {
    struct Wide w;
    w.x = s.p;
    w.y = s.q;
    w.z = s.p + s.q;
    return w;
}

int sum(struct Big b) {
    int t = 0;
    int i = 0;
    while (i < 8) { t = t + b.a[i]; i = i + 1; }
    return t;
}

int main(void) {
    struct Big b;
    struct Big d;
    struct Wide w;
    struct Mix m;
    struct Small s;
    int t;

    b = fill(10);
    printf("fill  : %d %d %d %d\n", b.a[0], b.a[3], b.a[7], sum(b));

    d = twice(b);
    printf("twice : %d %d %d %d\n", d.a[0], d.a[3], d.a[7], sum(d));

    w = scale(1.5);
    printf("scale : %.2f %.2f %.2f\n", w.x, w.y, w.z);

    b = six(1, 2, 3, 4, 5, 6);
    printf("six   : %d %d %d %d %d %d %d %d\n",
           b.a[0], b.a[1], b.a[2], b.a[3], b.a[4], b.a[5], b.a[6], b.a[7]);

    m = make(7, 2.5, 'x');
    printf("make  : %d %.1f %s\n", m.n, m.d, m.s);

    s.p = 4;
    s.q = 9;
    w = spread(s);
    printf("spread: %.1f %.1f %.1f\n", w.x, w.y, w.z);

    // The result of one call passed straight into the next, so the returned
    // object becomes a memory argument without ever being named.
    t = sum(twice(fill(1)));
    printf("nested: %d\n", t);

    // A member read directly off the call, with no variable in between.
    printf("direct: %d\n", fill(100).a[5]);
    return 0;
}
