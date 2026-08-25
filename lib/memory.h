// memory.h - the mem* functions, where older programs look for them.
//
// It predates the standard, which put memset and its neighbours in <string.h>
// and never gave this one a place. Programs that were written before that, or
// against a System V or BSD system that kept both, include it and are refused
// by a compiler that ships only what the standard lists - which is a program
// turned away for being older than the header list rather than for anything
// it does.
//
// So it forwards. There is nothing here that <string.h> does not declare, and
// declaring the six a second time in their own file would be a second place to
// keep them right.

#ifndef CC1_MEMORY_H
#define CC1_MEMORY_H

#include <string.h>

#endif
