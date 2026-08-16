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

// A jmp_buf that lives in a frame instead of at file scope.
//
// This is the case that tells one compiler from another on Windows. The UCRT
// fills a jmp_buf with aligned xmm saves, so it wants the buffer on a
// sixteen-byte boundary - and a file-scope one lands there whatever the
// compiler does, while a local one only lands there if the compiler means it
// to. cc1 could not align anything past eight until it learned to give every
// object of sixteen bytes or more sixteen; before that, this function was an
// access violation inside setjmp and the one above was fine.
//
// 'shove' is here to stop the frame from being accidentally arranged so that
// the buffer is aligned anyway.
int recover_locally(int code) {
    char shove;
    jmp_buf here;
    int r;

    shove = (char)code;
    r = setjmp(here);
    if (r == 0) {
        if (code != 0) longjmp(here, code);
        return (int)shove - (int)shove;   // the no-jump path, and it is 0
    }
    return r;
}

int jmp_buf_bytes(void) { return (int)sizeof(jmp_buf); }

// Cast through 'unsigned long long' rather than 'unsigned long', because long
// is four bytes on Windows and would throw half the address away before the
// remainder was taken - which would report a wrong answer rather than fail.
int jmp_buf_is_16_aligned(void) {
    return ((unsigned long long)(void *)env) % 16 == 0;
}
