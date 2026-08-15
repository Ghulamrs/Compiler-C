// expect: 0
// A switch whose arms are floating point, which is the shape that was reported
// against Xcode and the one tests/arm64/switch.c does not have: the subject
// travels in x0 and the value assigned in each arm travels in d0, so the two
// register files have to be kept apart across the compare-and-branch chain.
//
// The compound assignments below are here for the same reason - the rewrite
// 'x = x op e' reaches through a subscript to a double, so the address is
// computed twice and the arithmetic happens once, in the other register file.
int printf(const char *fmt, ...);

int main(void)
{
    int k;
    double z;
    double A[3][3];
    double b[3];
    double x[3];
    int i, j;
    int bad;

    bad = 0;

    for (k = 0; k < 3; k++) {
        z = 1.0;
        switch (k) {
        case 0:  z = 5.0;  break;
        case 1:  z = 10.0; break;
        default: z = 15.0; break;
        }
        if (k == 0 && z != 5.0)  bad = bad + 1;
        if (k == 1 && z != 10.0) bad = bad + 2;
        if (k == 2 && z != 15.0) bad = bad + 4;
    }

    /* a switch on an int that accumulates into a double, so the subject and
       the accumulator are live across the same branch */
    z = 0.0;
    for (k = 0; k < 5; k++) {
        switch (k % 3) {
        case 0:  z += 1.5;  break;
        case 1:  z += 0.25; break;
        default: z -= 0.75; break;
        }
    }
    /* k%3 over k=0..4 is 0,1,2,0,1: +1.5 +0.25 -0.75 +1.5 +0.25 */
    if (z != 2.75) bad = bad + 8;

    for (i = 0; i < 3; i++) {
        b[i] = i + 1;
        x[i] = 0.0;
        for (j = 0; j < 3; j++)
            A[i][j] = i * 3 + j + 1;
    }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            x[i] += A[i][j] * b[j];
    if (x[0] != 14.0 || x[1] != 32.0 || x[2] != 50.0) bad = bad + 16;

    if (bad != 0) printf("switch_fp: %d\n", bad);
    return bad;
}
