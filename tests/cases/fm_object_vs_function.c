// expect: 11
/* The space decides: "A (x)" is object-like with a body starting in '(', and
   "B(x)" is function-like. */
int printf(char *fmt, ...);
#define A (3)
#define B(x) ((x) + 1)
int main(void)
{
    return A + B(7);
}
