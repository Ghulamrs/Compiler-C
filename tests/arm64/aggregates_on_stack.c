// expect: 0
// Aggregates that do not fit in the registers left for them, which is the last
// thing this backend refused. Three rules meet here and they are genuinely
// three, all read off clang rather than recalled:
//
//   a named scalar on the stack takes its own size at its own alignment
//   a named aggregate is rounded up to a multiple of eight and aligned to
//     at least eight - a 12-byte struct after a char starts at 8, not 4,
//     and occupies 16
//   anything variadic takes eight, whatever it is
//
// And an aggregate goes wholly in registers or wholly in memory, never split:
// when it goes to memory it closes *its own* register file to every later
// argument, while leaving the other one open.
#include <stdio.h>

struct S16 { long a, b; };
struct S12 { int a, b, c; };
struct H3  { double x, y, z; };
struct Big { long a, b, c, d; };

// Seven integer registers spent, then a two-word struct that cannot fit in the
// one remaining. The struct goes to memory and so does `after` - x7 is left
// unused, which is the rule that is easy to get wrong by filling it.
static long two_word(int a, int b, int c, int d, int e, int f, int g,
                     struct S16 s, int after)
{
    return a+b+c+d+e+f+g + s.a*10 + s.b*100 + after*1000;
}

// The same for the vector file, and the regression that matters most here:
// `n` arrives in x0 and is read *after* the stack arguments. Reading one of
// those through x0 destroys it, and the only symptom is this number.
static double hfa(double a, double b, double c, double d, double e, double f,
                  double g, struct H3 s, double later, int n)
{
    return a+b+c+d+e+f+g + s.x*10 + s.y*100 + s.z*1000 + later*2 + n;
}

// Too large for registers, so only a pointer to the caller's copy travels -
// and here even that has to go on the stack.
static long byref(int a, int b, int c, int d, int e, int f, int g, int h,
                  struct Big s, int n)
{
    return a+b+c+d+e+f+g+h + s.a + s.b*10 + s.c*100 + s.d*1000 + n*10000;
}

// The alignment case: a char lands at 0, and the 12-byte struct after it at 8.
static long odd(int a, int b, int c, int d, int e, int f, int g, int h,
                char c1, struct S12 s, int z)
{
    return a+b+c+d+e+f+g+h + c1 + s.a*10 + s.b*100 + s.c*1000 + z*10000;
}

// An integer aggregate spills, closing the integer file - but the double after
// it must still arrive in d0. Closing both would be the obvious wrong reading.
static long crossfile(int a, int b, int c, int d, int e, int f, int g,
                      struct S16 s, double d1, int n)
{
    return a+b+c+d+e+f+g + s.a + s.b*10 + (long)d1*100 + n*1000;
}

int main(void)
{
    struct S16 s; struct S12 t; struct H3 h; struct Big b;

    s.a = 1; s.b = 2;
    t.a = 3; t.b = 4; t.c = 5;
    h.x = 1.5; h.y = 2.5; h.z = 3.5;
    b.a = 1; b.b = 2; b.c = 3; b.d = 4;

    return (int)(two_word(1,2,3,4,5,6,7, s, 9)          - 9238)
         + (int)(hfa(1,2,3,4,5,6,7, h, 8.5, 3)          - 3813.0)
         + (int)(byref(1,2,3,4,5,6,7,8, b, 5)           - 54357)
         + (int)(odd(1,2,3,4,5,6,7,8, 'A', t, 6)        - 65531)
         + (int)(crossfile(1,2,3,4,5,6,7, s, 9.0, 4)    - 4949);
}
