// ctype.h - the character classifications.
//
// Prototypes only, the way <stdio.h> and <math.h> are: the implementations
// arrive from the host's C library at link time. All twelve are real exported
// functions on every one of the three targets, which is not true of everything
// in the standard library - see the note about the UCRT's inlined stdio in
// help/command-lines.md.
//
// Every one takes an **int**, not a char, and the reason is a trap rather than
// a curiosity. The argument must be representable as an unsigned char or be
// EOF, so the idiom is
//
//     while ((c = getchar()) != EOF) if (isspace(c)) ...
//
// with c an int. Passing a plain char is where this goes wrong: char is signed
// on all three of these targets, so a byte above 127 arrives negative, and a
// negative value that is not EOF is undefined behaviour. Cast through unsigned
// char - isspace((unsigned char)*p) - when walking a string.
#ifndef _CC1_CTYPE_H
#define _CC1_CTYPE_H

int isalnum(int c);
int isalpha(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

int tolower(int c);
int toupper(int c);

#endif
