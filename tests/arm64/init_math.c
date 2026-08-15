// expect: 0
// Brace elision and <math.h> on arm64, which exercises two things the other
// suites cannot reach here: the initialiser walk emitting stores through this
// backend's addressing, and doubles crossing the call boundary into libm in
// d0 and coming back the same way.
//
// No structs - this backend still refuses member access by name, so the struct
// forms of elision are covered by tests/cases on x86-64 instead.
#include <math.h>

int printf(const char *fmt, ...);

int main(void)
{
    double a[2][3] = {1,2,3,4,5,6};   /* flat, two rows of three */
    int    d[3][2] = {1,2,3};         /* short: the rest is zero */
    int    c[2][2][2] = {1,2,3,4,5,6,7,8};
    double inf[][2] = {1.5,2.5,3.5,4.5};
    int i, j, k;
    int bad;

    bad = 0;

    if (a[0][0] != 1.0 || a[0][2] != 3.0 || a[1][0] != 4.0 || a[1][2] != 6.0)
        bad = bad + 1;
    if (d[0][0] != 1 || d[1][0] != 3 || d[1][1] != 0 || d[2][0] != 0)
        bad = bad + 2;
    if (c[0][0][0] != 1 || c[1][1][1] != 8) bad = bad + 4;
    if ((int)(sizeof inf / sizeof inf[0]) != 2 || inf[1][1] != 4.5)
        bad = bad + 8;

    if (sqrt(4.0) != 2.0 || pow(2.0, 10.0) != 1024.0) bad = bad + 16;
    if (floor(-2.5) != -3.0 || ceil(-2.5) != -2.0)    bad = bad + 32;
    if (fabs(-3.5) != 3.5 || fmod(7.0, 3.0) != 1.0)   bad = bad + 64;

    /* the sum of the whole array, so every element is read back */
    {
        double s = 0.0;
        for (i = 0; i < 2; i++)
            for (j = 0; j < 3; j++)
                s += a[i][j];
        if (s != 21.0) bad = bad + 128;
    }
    k = 0;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 2; j++)
            k += d[i][j];
    if (k != 6) bad = bad + 256;

    if (bad != 0) printf("init_math: %d\n", bad);
    return bad;
}
