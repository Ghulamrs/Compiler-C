// expect: 0
// Array and struct initialisers, at both storage durations. gcc builds the same
// file, so every layout decision here - where a member sits, what a short list
// leaves behind, how wide an inferred array is - is checked against glibc's
// compiler rather than against my opinion.
#include <stdio.h>

struct P { int x; int y; };
struct Q { char tag; int n; double d; };
struct Nest { struct P p; int a[3]; };
union U { int i; char c[4]; };

int      g1[4]     = {1, 2, 3, 4};
int      g2[5]     = {1, 2};              /* the rest is zero */
int      g3[]      = {7, 8, 9};           /* length counted from the list */
char     gs[8]     = "abc";               /* and zero to the end */
char     gs2[]     = "hello";             /* length measured, zero included */
struct P gp        = {10, 20};
struct Q gq        = {'A', 5, 1.5};       /* a double laid out as data */
struct P gpa[2]    = {{1, 2}, {3, 4}};
int      gm[2][3]  = {{1, 2, 3}, {4, 5, 6}};
union  U gu        = {66};                /* the first member, which is all C90 has */
static int gstatic[3] = {4, 5, 6};

// Fill a frame with a pattern, so that the frame reused below starts dirty.
// Without this, "the initialiser zeroed the rest" and "the stack happened to be
// zero" look the same - which they did, until an injection that skipped the
// terminating zero passed this case.
void dirty(void) {
    int junk[64];
    int i;

    i = 0;
    while (i < 64) { junk[i] = -1; i = i + 1; }
}

// Declared in a frame the caller has just dirtied, and printed byte by byte
// rather than as a string, so every byte the initialiser was supposed to write
// is looked at.
void fills(void) {
    char s[8] = "hi";
    int  n[6] = {1, 2};
    int i;

    printf("bytes:");
    for (i = 0; i < 8; i = i + 1) printf(" %d", s[i]);
    printf("\nrest :");
    for (i = 0; i < 6; i = i + 1) printf(" %d", n[i]);
    printf("\n");
}

int main(void) {
    int a[4] = {1, 2, 3, 4};
    int b[5] = {9, 8};
    int c[]  = {5, 6, 7};
    char s[8] = "hi";
    char t[]  = "world";
    struct P p = {11, 22};
    struct Q q = {'Z', 7, 2.5};
    struct Nest n = {{1, 2}, {3, 4, 5}};
    struct P pa[2] = {{1, 2}, {3, 4}};
    int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
    int one = {42};                        /* braces round a scalar are legal */
    struct P zeroed = {0};                 /* the idiom: first member, rest zero */
    int i, j;

    printf("a :"); for (i = 0; i < 4; i = i + 1) printf(" %d", a[i]); printf("\n");
    printf("b :"); for (i = 0; i < 5; i = i + 1) printf(" %d", b[i]); printf("\n");
    printf("c : %d %d %d len=%d\n", c[0], c[1], c[2], (int)sizeof c);
    printf("s : %s len=%d last=%d\n", s, (int)sizeof s, s[7]);
    printf("t : %s len=%d\n", t, (int)sizeof t);
    printf("p : %d %d\n", p.x, p.y);
    printf("q : %d %d %.1f\n", q.tag, q.n, q.d);
    printf("n : %d %d | %d %d %d\n", n.p.x, n.p.y, n.a[0], n.a[1], n.a[2]);
    printf("pa: %d %d %d %d\n", pa[0].x, pa[0].y, pa[1].x, pa[1].y);
    printf("m :");
    for (i = 0; i < 2; i = i + 1)
        for (j = 0; j < 3; j = j + 1) printf(" %d", m[i][j]);
    printf("\n");
    printf("one=%d zeroed=%d,%d\n", one, zeroed.x, zeroed.y);

    printf("g1:"); for (i = 0; i < 4; i = i + 1) printf(" %d", g1[i]); printf("\n");
    printf("g2:"); for (i = 0; i < 5; i = i + 1) printf(" %d", g2[i]); printf("\n");
    printf("g3: %d %d %d len=%d\n", g3[0], g3[1], g3[2], (int)sizeof g3);
    printf("gs: %s len=%d last=%d\n", gs, (int)sizeof gs, gs[7]);
    printf("gs2: %s len=%d\n", gs2, (int)sizeof gs2);
    printf("gp: %d %d\n", gp.x, gp.y);
    printf("gq: %d %d %.1f\n", gq.tag, gq.n, gq.d);
    printf("gpa: %d %d %d %d\n", gpa[0].x, gpa[0].y, gpa[1].x, gpa[1].y);
    printf("gm:");
    for (i = 0; i < 2; i = i + 1)
        for (j = 0; j < 3; j = j + 1) printf(" %d", gm[i][j]);
    printf("\n");
    printf("gu: %d %d\n", gu.i, gu.c[0]);
    printf("gstatic: %d %d %d\n", gstatic[0], gstatic[1], gstatic[2]);

    dirty();
    fills();
    return 0;
}
