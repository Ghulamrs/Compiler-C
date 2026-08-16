//
//  risky.h
//  CC1Lab
//
//  Created by G. R. Akhtar on 16/08/2026.
//


#ifndef RISKY_H
#define RISKY_H

#include <setjmp.h>

// Declare the environment buffer as extern
extern jmp_buf env;

// Function prototype
void risky_operation(int code);

// The same idea with the buffer in a frame rather than at file scope. It is a
// separate function because that is the only way to have one: a jmp_buf that
// setjmp is called on has to outlive nothing, but it must belong to a function
// that is still running when the longjmp happens.
int recover_locally(int code);

// How large this platform's jmp_buf is, and whether the one above came out on
// a sixteen-byte boundary. Both are answered from inside risky.c so that main
// does not have to know which platform it is on.
int  jmp_buf_bytes(void);
int  jmp_buf_is_16_aligned(void);

#endif // !RISKY_H
