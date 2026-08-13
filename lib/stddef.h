// stddef.h - the two things every other header here needs.
//
// It exists so that size_t is declared once. A typedef repeated in two headers
// is a redefinition, and a program including both would be refused for a
// mistake it did not make.
#ifndef _CC1_STDDEF_H
#define _CC1_STDDEF_H

// unsigned long, because this compiler targets x86-64 Linux and the System V
// LP64 model makes size_t 64 bits. It is not asked of Target here: a header is
// text, and text cannot consult the type table. The suite is what checks the
// two agree - a case printing sizeof(size_t) is built by gcc as well, and gcc
// reads glibc's stddef.h rather than this one.
typedef unsigned long size_t;

// glibc spells this ((void *)0). Here it is 0, because a null pointer constant
// in C is an integer constant expression with value zero, and this compiler
// accepts that in every place a pointer is wanted. The difference shows only
// where NULL is passed to a variadic function, which is a mistake in either
// spelling.
#define NULL 0

#endif
