//
//  mathops.c
//  CC1Lab
//
//  Created by G. R. Akhtar on 16/08/2026.
//


#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mathops.h"

mathfunc_t get_trig_function(const char *name) {
    if (strcmp(name, "sin") == 0) return sin;
    if (strcmp(name, "cos") == 0) return cos;
    if (strcmp(name, "tan") == 0) return tan;
    return NULL;
}

void matmult(double A[3][3], double B[3][3], double C[3][3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < 3; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}
