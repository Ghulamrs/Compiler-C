// expect: 0
// A struct or union as the arms of '?:'. This needed no code generation at all:
// an arm of struct type already leaves an address in %rax, which is what every
// other struct expression here yields, so the conditional was type-agnostic
// before it was allowed to see one.
//
// The arms must be the same type. Types are interned, so 'the same' is one
// pointer comparison, and the rule that was already there covers it.
#include <stdio.h>

struct Small { int p, q; };                 /* register class */
struct Big   { int a[8]; };                 /* memory class */
union  Cell  { int i; double d; };

int sumSmall(struct Small s) { return s.p + s.q; }
int sumBig(struct Big b) {
    int t = 0;
    int i = 0;
    while (i < 8) { t = t + b.a[i]; i = i + 1; }
    return t;
}

// Chosen in the callee, returned by value - the conditional feeding a return.
struct Big pick(int which, struct Big x, struct Big y) {
    return which ? x : y;
}

struct Small mk(int p, int q) {
    struct Small s;
    s.p = p;
    s.q = q;
    return s;
}

int main(void) {
    struct Small s1, s2, sr;
    struct Big b1, b2, br;
    union Cell c1, c2, cr;
    int i;

    s1 = mk(1, 2);
    s2 = mk(30, 40);

    i = 0;
    while (i < 8) { b1.a[i] = i; b2.a[i] = i * 100; i = i + 1; }

    c1.i = 7;
    c2.d = 2.5;

    // Assignment from a conditional, both ways round.
    sr = 1 ? s1 : s2;
    printf("small1: %d %d\n", sr.p, sr.q);
    sr = 0 ? s1 : s2;
    printf("small0: %d %d\n", sr.p, sr.q);

    br = 1 ? b1 : b2;
    printf("big1  : %d %d %d\n", br.a[0], br.a[3], br.a[7]);
    br = 0 ? b1 : b2;
    printf("big0  : %d %d %d\n", br.a[0], br.a[3], br.a[7]);

    // A union, where the arms share storage and only the read decides.
    cr = 1 ? c1 : c2;
    printf("union1: %d\n", cr.i);
    cr = 0 ? c1 : c2;
    printf("union0: %.1f\n", cr.d);

    // Straight into a call: a register-class struct and a memory-class one,
    // each chosen by a conditional that is never given a name.
    printf("callS : %d\n", sumSmall(1 ? s1 : s2));
    printf("callB : %d\n", sumBig(0 ? b1 : b2));

    // The conditional inside the callee, feeding a return by value.
    br = pick(1, b1, b2);
    printf("pick1 : %d %d\n", br.a[1], br.a[7]);
    br = pick(0, b1, b2);
    printf("pick0 : %d %d\n", br.a[1], br.a[7]);

    // A runtime condition rather than a constant, so nothing is folded away.
    i = 3;
    sr = (i > 2) ? s1 : s2;
    printf("runtime: %d %d\n", sr.p, sr.q);

    // A member read straight off the conditional, with no variable between.
    // The result is not an lvalue - taking its address is refused - but it has
    // an address to read a member through, which is all this needs.
    printf("member: %d %d\n", (i > 2 ? s1 : s2).q, (0 ? b1 : b2).a[5]);
    printf("nested: %d\n", (1 ? mk(5, 6) : mk(7, 8)).p);
    return 0;
}
