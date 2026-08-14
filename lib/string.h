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
// memmove is memcpy for the case memcpy refuses to define: a source and a
// destination that overlap. Sliding a run of bytes along inside one buffer is
// the ordinary reason to want it, and doing that with memcpy is undefined even
// where it happens to work.
void *memmove(void *, const void *, size_t);
int memcmp(const void *, const void *, size_t);
void *memchr(const void *, int, size_t);

// Searching. The three that return a pointer into their argument return null
// when they find nothing, which is why NULL is in <stddef.h> and reachable from
// here.
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strstr(const char *, const char *);
char *strpbrk(const char *, const char *);
size_t strspn(const char *, const char *);
size_t strcspn(const char *, const char *);

// strtok keeps state between calls, which makes it the one function here that
// is not reentrant. It is in C90 and so it is declared; a program that wants
// two scans at once needs something else.
char *strtok(char *, const char *);

char *strerror(int);

#endif
