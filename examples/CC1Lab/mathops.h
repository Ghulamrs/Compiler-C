//
//  mathops.h
//  CC1Lab
//
//  Created by G. R. Akhtar on 16/08/2026.
//


#ifndef MATHOPS_H
#define MATHOPS_H

// Function pointer type: takes double, returns double
typedef double (*mathfunc_t)(double);

// Function returning a function pointer
mathfunc_t get_trig_function(const char *name);

// Matrix multiplication
void matmult(double A[3][3], double B[3][3], double C[3][3]);

#endif // !MATHOPS_H
