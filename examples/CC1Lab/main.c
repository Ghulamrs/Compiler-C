//
//  main.c
//  CC1Lab
//
//  Created by G. R. Akhtar on 16/08/2026.
//


#include <stdio.h>
#include <math.h>
#include "risky.h"

int main(void) {
    int status = setjmp(env);

    if (status == 0) {
        printf("Entering risky operations...\n");

        risky_operation(0);   // success
        risky_operation(1);   // triggers longjmp
        risky_operation(2);   // never reached

        printf("This line will not execute after longjmp.\n");
    } else {
        printf("Recovered from error, status=%d\n", status);

        // Demonstrate numeric types after recovery
        float f = 3.14f;
        double d = cos(0.5);
        long l = 123456L;
        long double ld = 2.718281828459045L;

        printf("Values after recovery:\n");
        printf(" float=%.2f\n", f);
        printf(" double=%.6f\n", d);
        printf(" long=%ld\n", l);
        printf(" long double=%.10Lf\n", ld);
    }

    return 0;
}
