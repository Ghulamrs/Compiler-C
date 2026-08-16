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

        // What this platform's jmp_buf actually is. The number differs by
        // target and is meant to - 192 bytes on macOS arm64, 200 on Linux, 256
        // under the UCRT - because setjmp lives in the C library and the
        // buffer has to be as large as that library was built to fill.
        printf("jmp_buf: %d bytes, 16-byte aligned=%d\n",
               jmp_buf_bytes(), jmp_buf_is_16_aligned());

        // The buffer in a frame rather than at file scope. On Windows this is
        // the one that faults if the compiler cannot align a local past eight
        // bytes, and it is the reason the alignment rule exists at all.
        printf("local buffer: no-jump=%d jumped=%d\n",
               recover_locally(0), recover_locally(9));
    }

    return 0;
}
