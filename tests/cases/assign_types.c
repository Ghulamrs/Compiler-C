// expect: 0
// The same boundary as arg_types.c, from the other two places C converts "as if
// by assignment": '=' itself, the initialiser written with a declaration, and
// return. One rule, three doorways, and this pins the conversions that must
// keep working through each of them.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char *identity(char *p) { return p; }        // return, pointer to same pointer
long  widen(void)       { return 1; }        // return, int to long
int   narrow(void)      { return 'a'; }      // return, char constant to int
double as_double(void)  { return 3; }        // return, int to double

int main(void) {
    char buf[4];
    char *p = "literal";                     // initialiser, array to pointer
    char *q = 0;                             // initialiser, null constant
    char *r = NULL;                          // initialiser, NULL
    void *v = malloc(4);                     // initialiser, same type
    char *w = malloc(4);                     // initialiser, void * to char *
    long  n = 5;                             // initialiser, int to long
    double d = 2;                            // initialiser, int to double
    int    i;

    printf("%s %d %d %d\n", p, q == 0, r == 0, v != 0);

    q = buf;                                 // '=', array decays
    q = p;                                   // '=', same type
    q = 0;                                   // '=', null constant
    q = v;                                   // '=', void * in
    v = w;                                   // '=', char * to void *
    n = 7;                                   // '=', arithmetic
    d = n;                                   // '=', long to double
    i = (int)d;                              // '=', through an explicit cast

    printf("%d %ld %.1f %d\n", q == 0, n, d, i);
    printf("%ld %d %.1f %d\n", widen(), narrow(), as_double(),
           identity(buf) == buf);

    free(w);
    return 0;
}
