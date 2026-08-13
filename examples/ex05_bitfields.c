// Bit-fields: packed from the low bit up, never straddling a boundary of their
// declared type, read with two shifts and written with a read-modify-write so
// the neighbours survive. A bit-field is an lvalue with no address.
#include <stdio.h>
#include "examples.h"

struct Flags {
    unsigned int ready   : 1;
    unsigned int mode    : 3;
    unsigned int level   : 4;
    int          bias    : 3;    /* signed, so 7 reads back as -1 */
    unsigned int         : 0;    /* forces the next field to a fresh unit */
    unsigned int tail    : 5;
};

struct Mixed { char tag; unsigned int a : 6; unsigned int b : 6; int n; };

int ex_bitfields(void) {
    struct Flags f;
    struct Mixed m;

    printf("[bitfields]\n");
    f.ready = 1; f.mode = 5; f.level = 9; f.bias = 7; f.tail = 31;
    printf("  fields   : %u %u %u %d %u\n", f.ready, f.mode, f.level, f.bias, f.tail);
    printf("  size     : %d (the ': 0' starts a new unit)\n", (int)sizeof f);

    // Writing more than fits keeps the low bits, and the value of the
    // assignment is what was stored rather than what was given.
    f.mode = 300;
    printf("  wrapped  : %u\n", f.mode);

    // The neighbours must survive a write.
    f.ready = 0;
    printf("  neighbour: %u %u %u\n", f.ready, f.mode, f.level);

    m.tag = 'Z'; m.a = 63; m.b = 1; m.n = -5;
    printf("  mixed    : %d %u %u %d size=%d\n", m.tag, m.a, m.b, m.n, (int)sizeof m);
    m.a = m.a - 1;
    printf("  read-mod : %u %u\n", m.a, m.b);
    return 6;
}
