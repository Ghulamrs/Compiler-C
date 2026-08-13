// string.h - the char * functions, and the memory ones beside them.
//
// The same bargain as <stdio.h>: prototypes only, checked against glibc by the
// suite building every case twice. See that file for why this is not a copy of
// the real header.
//
// const on a parameter is written as C writes it, and is worth one honest note.
// In this compiler const is a property of the declared object rather than part
// of its type, so "const char *" makes neither the pointer nor what it points
// at read-only here. The qualifier is accepted and carried; it simply does not
// yet reach through a pointer. Writing these prototypes any other way would
// misdescribe the functions to a reader for the sake of matching what the type
// model currently checks.
#ifndef _CC1_STRING_H
#define _CC1_STRING_H

#include <stddef.h>

size_t strlen(const char *);

char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
char *strcat(char *, const char *);

int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);

#endif
