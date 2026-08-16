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

#endif // !RISKY_H
