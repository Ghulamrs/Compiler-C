// examples.h - the ten demonstrations, and nothing else.
//
// Each returns the number of checks it ran, so main can add them up and a
// wrong answer shows as a different total rather than as silence.
#ifndef EXAMPLES_H
#define EXAMPLES_H

int ex_arithmetic(void);
int ex_control(void);
int ex_pointers(void);
int ex_structs(void);
int ex_bitfields(void);
int ex_strings(void);
int ex_floats(void);
int ex_fnptr(void);
int ex_initialisers(void);
int ex_fileio(void);

// The long one. Compiled for the sake of compiling it; see heavy.c.
int heavy_main(void);

#endif
