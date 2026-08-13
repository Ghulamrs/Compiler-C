// stdio.h - the part of it this compiler can hold.
//
// Not glibc's header, and not a copy of one. Reaching glibc's <stdio.h> means
// reading 24 files and 744 lines carrying 107 uses of __restrict, 57 of
// __attribute__, 15 of long double and a scattering of __extension__, __asm__,
// __inline and __typeof - none of which is C, and all of which this compiler
// would have to swallow before it reached a single declaration it could use.
//
// What a program actually wants from stdio.h is the prototypes, and a prototype
// is ordinary C.
//
// These declarations must agree with glibc's, because the program links against
// glibc. Nothing here is taken on trust: every test case is built a second time
// by gcc, which reads the real header, and the two binaries must print the same
// bytes and exit with the same status. A prototype that lied here would fail
// there - which is why the suite deliberately does not pass -I to gcc.
//
// FILE and everything taking one is absent. An opaque struct needs an
// incomplete type, which this compiler does not have.
#ifndef _CC1_STDIO_H
#define _CC1_STDIO_H

int printf(const char *, ...);
int sprintf(char *, const char *, ...);
int puts(const char *);
int putchar(int);
int getchar(void);

#endif
