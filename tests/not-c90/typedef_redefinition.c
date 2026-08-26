/* Redeclaring a typedef with the same type is C11. C90 forbids it outright,
   and gcc -std=c90 -pedantic says so.

   This file is here for a second reason as well, and it is the stronger one.
   Until 2026-08-26 cc1 did not refuse this - it did not answer at all. The
   specifier loop in Parser::specifiers took 'atTypeName()' as its condition,
   which is true for an identifier naming a typedef, while nothing inside the
   loop consumed one; so the second 'T' spun forever. The 'typedefed twice'
   error two screens below it had never once been reached.

   Nothing could see that. All 425 differential cases compile, so none of them
   redeclares anything, and a compiler that hangs fails no suite - it simply
   never returns. The entry below reading "refuses" is what now says the
   diagnostic is reachable, and a regression would show up as this probe
   failing to finish rather than as a wrong answer. */
typedef long T;
typedef long T;

int main(void)
{
    T x = 1;
    return (int)x;
}
