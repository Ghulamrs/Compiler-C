// stdlib.h - allocation, conversion, and leaving.
//
// The same bargain as <stdio.h>: prototypes only, checked against glibc by the
// suite building every case twice. See that file for why this is not a copy of
// the real header.
//
// malloc needed nothing new from the compiler when it first appeared here - it
// arrives through an ordinary prototype, and the cast that turns its block into
// an array of something is the declarator grammar doing its ordinary work.
#ifndef _CC1_STDLIB_H
#define _CC1_STDLIB_H

#include <stddef.h>

void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);

void exit(int);
void abort(void);

int atoi(const char *);
long atol(const char *);

int abs(int);
long labs(long);

int rand(void);
void srand(unsigned int);

#endif
