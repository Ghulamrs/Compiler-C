// expect: 0
// 'long double' crossing a function boundary, which on System V is where it
// stops resembling every other arithmetic type.
//
// Its classes are X87 and X87UP, and neither names a register a caller may put
// it in: it travels on the stack always, sixteen-aligned, and comes back in
// st(0) rather than in %xmm0. An aggregate carrying one is MEMORY whatever its
// size - so 'struct { long double x; }' is exactly sixteen bytes, inside the
// limit that would otherwise return it in registers, and still goes through
// the hidden pointer.
//
// The alignment is the part that cannot be got wrong quietly. glibc's own
// va_arg rounds the overflow pointer up to sixteen before reading a long
// double, so a caller that leaves one at an odd slot - after a single int,
// say - has printf read eight bytes of the neighbour. That is what the mixed
// call at the end is for.
#include <stdio.h>

struct One   { long double x; };
struct Mixed { int a; long double x; double d; };

static long double add3(long double p, long double q, long double s) {
    return p + q + s;
}
static long double afterInt(int n, long double v) { return v * n; }
static struct One  makeOne(long double v)  { struct One o; o.x = v; return o; }
static long double takeOne(struct One o)   { return o.x + 1.0L; }
static long double takeMixed(struct Mixed m) { return m.x + m.a + m.d; }

// A callee that writes through its own copy: both by-reference ABIs make the
// *caller* copy an aggregate, and one that skips the copy passes everything
// else and fails only this.
static long double scribble(struct Mixed m) {
    m.x = 0.0L;
    m.a = 0;
    return m.x + m.a;
}

int main(void) {
    struct One o;
    struct Mixed m;
    int bad = 0;

    if (add3(1.5L, 2.25L, 4.0L) != 7.75L) bad++;
    if (afterInt(3, 1.5L) != 4.5L) bad++;

    o = makeOne(9.75L);
    if (o.x != 9.75L) bad++;
    if (takeOne(o) != 10.75L) bad++;

    m.a = 7; m.x = 1.25L; m.d = 0.5;
    if (takeMixed(m) != 8.75L) bad++;
    if (scribble(m) != 0.0L) bad++;
    if (m.a != 7 || m.x != 1.25L) bad++;      // the caller's copy survived

    // A long double at an odd stack slot, and one after arguments that have
    // spent the integer registers. Both are read back by printf rather than by
    // this program, which is what makes the ABI the thing under test.
    printf("%d %.4Lf %s %.4Lf\n", 1, 2.5L, "x", 3.5L);
    printf("%d %d %d %d %d %.4Lf\n", 1, 2, 3, 4, 5, 6.5L);
    printf("%.4Lf %d %.4Lf\n", 1.5L, 2, 3.5L);

    return bad;
}
