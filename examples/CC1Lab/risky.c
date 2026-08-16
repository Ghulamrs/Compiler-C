//
//  risky.c
//  CC1Lab
//
//  Created by G. R. Akhtar on 16/08/2026.
//


#include <stdio.h>
#include <setjmp.h>
#include "risky.h"

// Define the environment buffer here
jmp_buf env;

void risky_operation(int code) {
    if (code == 0) {
        printf("risky_operation: success\n");
    } else {
        printf("risky_operation: error, jumping back!\n");
        longjmp(env, code);  // non-local jump
    }
}
