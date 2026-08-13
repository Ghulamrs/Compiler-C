// expect: 0
// One caller for all ten examples, so the whole set is compiled as separate
// translation units and linked - which is what C is arranged around, and what
// makes this a load test of the driver as well as of the language.
//
//   ../cc1 *.c            one invocation, every file, threads if the machine
//                         has the cores for it
//   ../cc1 -j 1 *.c       the same thing serially, for when something is wrong
//
// Each example returns how many checks it ran. main adds them up, so a total
// that is not 100 means something went quiet rather than wrong.
#include <stdio.h>
#include "examples.h"

int main(void) {
    int total = 0;

    total = total + ex_arithmetic();
    total = total + ex_control();
    total = total + ex_pointers();
    total = total + ex_structs();
    total = total + ex_bitfields();
    total = total + ex_strings();
    total = total + ex_floats();
    total = total + ex_fnptr();
    total = total + ex_initialisers();
    total = total + ex_fileio();
    total = total + heavy_main();

    printf("\n[total] %d checks across 10 examples and heavy.c\n", total);
    return total == 100 ? 0 : 1;
}
