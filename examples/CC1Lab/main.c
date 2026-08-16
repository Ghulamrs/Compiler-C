#include <stdio.h>
#include <math.h>
#include "mathops.h"
#include "types.h"

int main(void) {
    // Struct usage
    NumSet ns = {3.14f, 2.718281828, 42L, 1234567890123LL, 1.6180339887L};
    printer_t p = print_numset;
    p(ns);

    // Union usage
    IntFloatUnion u;
    u.i = 1065353216; // bit pattern for float 1.0
    printf("\nUnion reinterpretation: int=%d, float=%f\n", u.i, u.f);

    // Function returning function pointer
    mathfunc_t fn = get_trig_function("cos");
    if (fn) {
        double val = fn(M_PI / 3);
        printf("\ncos(pi/3) = %.6f\n", val);
    }

    // Matrix multiplication
    double A[3][3] = {{1,2,3},{4,5,6},{7,8,9}};
    double B[3][3] = {{9,8,7},{6,5,4},{3,2,1}};
    double C[3][3];
    matmult(A,B,C);

    printf("\nMatrix multiplication result:\n");
    for (int i=0;i<3;i++) {
        for (int j=0;j<3;j++) {
            printf("%8.2f ", C[i][j]);
        }
        printf("\n");
    }

    return 0;
}
