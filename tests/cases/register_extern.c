// expect: 0
// Two storage classes that were refused for opposite reasons. 'register' was
// refused because this compiler cannot honour it; 'extern' on a local because
// nothing resolved the name. Both are now accepted, and only one of them has
// any observable rule attached - a register object has no address.
#include <stdio.h>

int counter;
int table[4];
double ratio;

void bump(void) {
    // The definitions are at file scope; these name them without redeclaring.
    extern int counter;
    extern int table[4];
    counter = counter + 1;
    table[counter] = counter * 10;
}

// A register parameter, and a register local doing the work.
int total(register int n) {
    register int i;
    register long acc;
    acc = 0;
    i = 0;
    while (i < n) { acc = acc + i; i = i + 1; }
    return (int)acc;
}

// register on something that still lives in memory here, which is the whole
// admission: the hint is taken by nobody and changes no output.
int spread(void) {
    register double d;
    register char c;
    d = 2.5;
    c = 'A';
    return (int)(d * 2.0) + (c - 'A');
}

// An extern declared before the file-scope definition it names.
int later(void) {
    extern double ratio;
    return (int)(ratio * 4.0);
}

int main(void) {
    register int k;
    int addressable;
    int *p;

    counter = 0;
    bump();
    bump();
    bump();
    printf("bump  : %d %d %d %d\n", counter, table[1], table[2], table[3]);

    printf("total : %d\n", total(10));
    printf("spread: %d\n", spread());

    ratio = 1.25;
    printf("later : %d\n", later());

    k = 7;
    printf("reg   : %d\n", k * 3);

    // A register object cannot have its address taken, but an ordinary one
    // sitting beside it still can.
    addressable = 41;
    p = &addressable;
    *p = *p + 1;
    printf("addr  : %d\n", addressable);
    return 0;
}
