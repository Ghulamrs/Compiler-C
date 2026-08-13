// expect: 0
// Arguments past the registers, which is one mechanism wearing two names: more
// than six integer parameters, and a struct too large to travel in registers.
// System V puts both in the same place - memory, laid out upwards from the
// callee's 16(%rbp) - so both stopped being refused at once.
//
// gcc builds this file too, and it is the check that matters here more than
// anywhere: the caller and the callee must agree exactly on which argument
// went where. Disagree by one slot and the program runs and returns nonsense.
#include <stdio.h>

struct Big   { int a[8]; };          /* 32 bytes - MEMORY class outright */
struct Wide  { double x, y, z; };    /* 24 bytes - MEMORY, and floating */
struct Small { int p; int q; };      /* 8 bytes - still travels in a register */

int eight(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b + c + d + e + f + g + h;
}

int twelve(int a, int b, int c, int d, int e, int f,
           int g, int h, int i, int j, int k, int l) {
    return a + b + c + d + e + f + g + h + i + j + k + l;
}

// Ten doubles: the SSE file holds eight, so the last two go to memory.
double tenD(double a, double b, double c, double d, double e,
            double f, double g, double h, double i, double j) {
    return a + b + c + d + e + f + g + h + i + j;
}

// The two files run out at different points, so this crosses the boundary in
// one lane while the other still has room.
int mixed(int a, double b, int c, double d, int e,
          double f, int g, double h, int i, double j) {
    return a + c + e + g + i + (int)(b + d + f + h + j);
}

int bigsum(struct Big v) {
    int s = 0, i;
    for (i = 0; i < 8; i = i + 1) s = s + v.a[i];
    return s;
}

double widesum(struct Wide w) { return w.x + w.y + w.z; }

// A register struct, a memory struct and a scalar in one call, so the two
// paths are exercised against each other rather than one at a time.
int both(struct Small s, struct Big b, int extra) {
    int t = 0, i;
    for (i = 0; i < 8; i = i + 1) t = t + b.a[i];
    return s.p + s.q + t + extra;
}

int main(void) {
    struct Big b;
    struct Wide w;
    struct Small s;
    int i;

    for (i = 0; i < 8; i = i + 1) b.a[i] = i + 1;
    w.x = 1.5; w.y = 2.25; w.z = 3.0;
    s.p = 10; s.q = 20;

    printf("eight  : %d\n", eight(1, 2, 3, 4, 5, 6, 7, 8));
    printf("twelve : %d\n", twelve(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12));
    printf("tenD   : %.2f\n", tenD(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    printf("mixed  : %d\n", mixed(1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5));
    printf("big    : %d\n", bigsum(b));
    printf("wide   : %.2f\n", widesum(w));
    printf("both   : %d\n", both(s, b, 100));

    // A variadic call over the boundary, where %al still has to be right.
    printf("many   : %d %d %d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6, 7, 8);
    printf("floats : %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f\n",
           1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    return 0;
}
