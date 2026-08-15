#include <stdio.h>
#include <math.h>

#define N 5

double trig_mix(double x, int depth) {
    if (depth <= 0) return x;
    if (depth % 2 == 0)
        return cos(trig_mix(x, depth - 1));
    else
        return sin(trig_mix(x, depth - 1));
}

void fill_matrix(double M[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double angle = (i * j + 1) * M_PI / 6.0;
            M[i][j] = cos(angle) + sin(angle) * tan(angle);
        }
    }
}

void print_matrix(double M[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            printf("%8.4f ", M[i][j]);
        }
        printf("\n");
    }
}

void matmult(double A[N][N], double B[N][N], double C[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main(void)
{
    double M1[N][N], M2[N][N], Result[N][N];

    fill_matrix(M1);
    fill_matrix(M2);

    printf("Matrix M1:\n");
    print_matrix(M1);

    printf("\nMatrix M2:\n");
    print_matrix(M2);

    matmult(M1, M2, Result);

    printf("\nResult of M1 * M2:\n");
    print_matrix(Result);

    double x = 1.2345;
    double identity = pow(cos(x), 2) + pow(sin(x), 2);
    printf("\nIdentity check at x=%.4f: %.6f\n", x, identity);

    double result = trig_mix(0.7, 6);
    printf("\nRecursive trig_mix(0.7, 6) = %.6f\n", result);

    double test_angle = 0.9;
    printf("\ncos(%.2f) = %.6f, cos(-%.2f) = %.6f\n",
           test_angle, cos(test_angle), test_angle, cos(-test_angle));

    return 0;
}
