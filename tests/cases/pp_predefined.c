// expect: 0
/* The macros the compiler defines before it reads a line. gcc predefines 368;
   this needs about a dozen, and the ones that matter are the two questions a
   header actually asks: which platform, and how wide is a long. <stdarg.h>
   asks both, because System V and Microsoft x64 disagree about what a va_list
   even is. */
#include <stdio.h>

int main(void)
{
    int seen = 0;

#if __STDC__ != 1
    return 1;
#endif
#ifdef _WIN32
    return 2;                       /* not this target */
#endif
#ifndef __linux__
    return 3;
#endif
#ifndef __x86_64__
    return 4;
#endif
    if (__SIZEOF_LONG__ != (int)sizeof(long)) return 5;
    if (__SIZEOF_POINTER__ != (int)sizeof(void *)) return 6;
    if (__SIZEOF_INT__ != (int)sizeof(int)) return 7;
    if (__CHAR_BIT__ != 8) return 8;

    /* They are ordinary macros, not a separate kind of thing: #undef reaches
       them exactly as it reaches a #define from this file. */
#undef __x86_64__
#ifdef __x86_64__
    return 9;
#endif
    seen = 1;
    printf("predefined macros agree with sizeof, and #undef reaches them\n");
    return seen - 1;
}
