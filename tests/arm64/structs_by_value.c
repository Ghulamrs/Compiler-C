// expect: 0
// Aggregates crossing a function boundary under AAPCS64, which has three rules
// where Microsoft x64 has one and System V has another again.
//
//   an HFA - one to four members all of the same floating type - travels in
//   that many consecutive vector registers, whatever its size, so four doubles
//   go in d0-d3 and come back the same way;
//   anything else of 16 bytes or less travels in one or two integer registers;
//   anything larger is copied by the caller and passed as a pointer, and
//   returned through a pointer the caller puts in x8.
//
// The sizes below are chosen to land on each of those, and on the edges: 12
// bytes is two registers holding only twelve bytes, so the second store has to
// be narrowed or it writes four bytes into whatever the frame keeps next door.
int printf(const char *fmt, ...);

struct P   { int x; int y; };            /* 8  - one register            */
struct Q   { int a; int b; int c; };     /* 12 - two, the second partial  */
struct M   { int n; double d; };         /* 16 - two, not an HFA          */
struct V   { double a; double b; };      /* 16 - an HFA of two            */
struct W   { double x, y, z; };          /* 24 - an HFA of three          */
struct F   { float a; float b; };        /* 8  - an HFA of two floats     */
struct Big { int a[8]; };                /* 32 - by pointer, x8 to return */

int    sump(struct P p)   { return p.x + p.y; }
int    sumq(struct Q q)   { return q.a + q.b + q.c; }
double summ(struct M m)   { return m.n + m.d; }
double sumv(struct V v)   { return v.a + v.b; }
double sumw(struct W w)   { return w.x + w.y + w.z; }
double sumf(struct F f)   { return f.a + f.b; }

struct P mkp(int x, int y)        { struct P p; p.x = x; p.y = y; return p; }
struct Q mkq(int a)               { struct Q q; q.a = a; q.b = a+1; q.c = a+2; return q; }
struct V mkv(double a, double b)  { struct V v; v.a = a; v.b = b; return v; }
struct W mkw(double k)            { struct W w; w.x = k; w.y = k*2; w.z = k*3; return w; }
struct Big fill(int base)         { struct Big b; int i = 0;
                                    while (i < 8) { b.a[i] = base + i; i = i + 1; }
                                    return b; }
int sumbig(struct Big b)          { int t = 0; int i = 0;
                                    while (i < 8) { t = t + b.a[i]; i = i + 1; }
                                    return t; }

/* writes over its own parameter: the caller's copy must not follow it */
int scribble(struct Big b) { int i = 0;
                             while (i < 8) { b.a[i] = 0; i = i + 1; }
                             return b.a[0]; }

/* two large ones and a chooser, so both arrive as pointers */
struct Big pick(int which, struct Big x, struct Big y) { return which ? x : y; }

/* an aggregate followed by scalars, which must take the registers after it */
int tail(struct Q q, int c, int d, int e) { return q.a + q.b + q.c + c + d + e; }

int main(void)
{
    struct P p; struct Q q; struct M m; struct V v; struct W w; struct F f;
    struct Big g, h, r;
    int bad, i;

    bad = 0;

    p.x = 3; p.y = 4;                    if (sump(p) != 7)     bad = bad + 1;
    q.a = 1; q.b = 2; q.c = 3;           if (sumq(q) != 6)     bad = bad + 2;
    m.n = 7; m.d = 0.5;                  if (summ(m) != 7.5)   bad = bad + 4;
    v.a = 1.5; v.b = 2.25;               if (sumv(v) != 3.75)  bad = bad + 8;
    w.x = 1.0; w.y = 2.0; w.z = 3.0;     if (sumw(w) != 6.0)   bad = bad + 16;
    f.a = 1.25f; f.b = 2.5f;             if (sumf(f) != 3.75)  bad = bad + 32;

    p = mkp(10, 20);
    if (p.x != 10 || p.y != 20) bad = bad + 64;

    q = mkq(5);
    if (q.a != 5 || q.b != 6 || q.c != 7) bad = bad + 128;

    v = mkv(0.5, 0.25);
    if (v.a != 0.5 || v.b != 0.25) bad = bad + 256;

    w = mkw(2.0);
    if (w.x != 2.0 || w.y != 4.0 || w.z != 6.0) bad = bad + 512;

    g = fill(100);
    if (g.a[0] != 100 || g.a[7] != 107) bad = bad + 1024;
    if (sumbig(g) != 828) bad = bad + 2048;          /* 100..107 */

    if (scribble(g) != 0) bad = bad + 4096;
    if (g.a[0] != 100 || g.a[7] != 107) bad = bad + 8192;   /* ours is intact */

    h = fill(200);
    r = pick(1, g, h);
    if (r.a[0] != 100) bad = bad + 16384;
    r = pick(0, g, h);
    if (r.a[0] != 200) bad = bad + 32768;

    if (tail(q, 10, 20, 30) != 78) bad = bad + 65536;  /* 5+6+7+10+20+30 */

    i = 0;
    if (i != 0) bad = bad + 131072;

    if (bad != 0) printf("structs_by_value: %d\n", bad);
    return bad;
}
