// stdio.h - the part of it this compiler can hold.
//
// Not glibc's header, and not a copy of one. Reaching glibc's <stdio.h> means
// reading 24 files and 744 lines carrying 107 uses of __restrict, 57 of
// __attribute__, 15 of long double and a scattering of __extension__, __asm__,
// __inline and __typeof - none of it C, and all of it in the way before a
// single usable declaration. What a program wants from stdio.h is the
// prototypes, and a prototype is ordinary C.
//
// These declarations must agree with glibc's, because the program links against
// glibc. Nothing here is taken on trust: every test case is built a second time
// by gcc, which reads the real header, and the two binaries must produce the
// same bytes and the same exit status. A prototype that lied would fail there -
// which is why the suite deliberately does not pass -I to gcc.
#ifndef _CC1_STDIO_H
#define _CC1_STDIO_H

#include <stddef.h>

// FILE is an incomplete struct reached only through a pointer, which is exactly
// how glibc declares it and exactly what this compiler supports. It was left
// out of this header for a while on the belief that an opaque handle needed
// something the compiler did not have; that was wrong, and the file functions
// below have worked from the day incomplete types did.
//
// The name inside the typedef is glibc's own. It need not be - nothing here can
// ever see the definition - but matching it means a reader comparing this file
// against the real one is not left wondering whether they are the same type.
typedef struct _IO_FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Opening and closing.
FILE *fopen(const char *, const char *);
FILE *freopen(const char *, const char *, FILE *);
FILE *tmpfile(void);
int fclose(FILE *);
int fflush(FILE *);

// Formatted output and input.
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int sprintf(char *, const char *, ...);
int scanf(const char *, ...);
int fscanf(FILE *, const char *, ...);
int sscanf(const char *, const char *, ...);

// Characters and lines.
int fgetc(FILE *);
int fputc(int, FILE *);
int getc(FILE *);
int putc(int, FILE *);
int getchar(void);
int putchar(int);
int ungetc(int, FILE *);
char *fgets(char *, int, FILE *);
int fputs(const char *, FILE *);
int puts(const char *);

// Whole blocks, which is what "binary mode" amounts to: bytes in and out with
// no interpretation. fwrite takes const void * and fread void *, and a struct
// reaches them through '&' like anything else.
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);

// Position.
int fseek(FILE *, long, int);
long ftell(FILE *);
void rewind(FILE *);

// State.
int feof(FILE *);
int ferror(FILE *);
void clearerr(FILE *);
void perror(const char *);

// The files themselves.
int remove(const char *);
int rename(const char *, const char *);

// Three absences, each for a reason rather than an oversight.
//
// vprintf, vfprintf and vsprintf take a va_list, and building one needs
// va_start, which needs a variadic function definition - which this compiler
// refuses by name. There is no way to call them correctly from C it accepts.
//
// fgetpos and fsetpos take an fpos_t *, and the caller has to declare the
// fpos_t. glibc's is a struct, so declaring one needs its definition, and its
// definition is the kind of thing this header exists to avoid. fseek and ftell
// do the same work with a long.
//
// setbuf and setvbuf are omitted only because nothing here has needed them; add
// them the day something does.

#endif
