/* A place to write C and find out what cc1 does with it.
 *
 * Build with Cmd-B. cc1 compiles this file; clang looks at it too and its
 * opinion appears in the log prefixed "clang says:". Where the two disagree is
 * a finding - either a gap in cc1 or a mistake in the program, and the two
 * verdicts side by side are what tell you which.
 *
 * Only the five headers cc1 ships are reachable: stdio.h, stdlib.h, string.h,
 * stddef.h, stdarg.h. There is no system include path, so the SDK is not on
 * the table.
 */
#include <stdio.h>

#define HELLO
double invert(double a[3][3]);
void matmul(double A[3][3], double b[3], double res[3]);

int main(void)
{
    int i, j;
    double q = 0.5;
    double A[3][3] = {
        { 4.0, -1.0,  0.0},
        {-1.0,  4.0, -1.0},
        { 0.0, -1.0,  4.0}
    };
    double b[3] = {2.0, 4.0, 10.0};
    double x[3];
    
    invert(A);
    matmul(A, b, x);
    for(i=0; i<3; i++) printf("%4.2lf\n", x[i]);
    for(j=0; j<10; j++) {
        switch(j+1) {
            case 1: q += 0.5;
                break;
            case 2: q += 1.5;
                break;
            case 3: q += 2.5;
                break;
            case 4: q += 3.5;
                break;
            default:
                q += 5.0;
                break;
        }
        printf("%1d. %3.1lf\n", j, q);
    }
    
    return 0;
}

double invert(double a[3][3])
{
    const double TOL = 1.0e-30;
    double determinant = 1.0;
    int i, j, k;
    double r;

    for (i = 0; i < 3; ++i) {
        determinant *= a[i][i];

        if ((determinant*determinant) <= TOL) {
            if (a[i][i] == 0.0) return determinant;
            determinant = TOL;
        }

        r = 1.0 / a[i][i];
        a[i][i] = 1.0;
        
        for (j = 0; j < 3; ++j)
            a[i][j] = r * a[i][j];

        for (k = 0; k < 3; ++k) {
            if (i != k && a[k][i] != 0.0) {
                r = a[k][i];
                a[k][i] = 0.0;

                for (j = 0; j < 3; ++j)
                    a[k][j] -= r * a[i][j];
            }
        }
    }

    return determinant;
}

void matmul(double A[3][3], double b[3], double res[3])
{
    int i, j;
    for(i=0; i<3; i++) {
        res[i] = 0.0;
        for(j=0; j<3; j++) {
        res[i] += A[i][j] * b[j];
        }
    }
}
