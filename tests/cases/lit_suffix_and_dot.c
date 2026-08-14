// expect: 0
/* Three things the literal lexer used to get wrong, each found by asking what
   C90 allows rather than by a program failing.

   .5 with no leading digit is a constant, and telling it from the member
   operator takes one character of lookahead - the digit after the dot.

   ULL is three suffix characters and the loop that read them stopped at two,
   so the L was left behind and the declaration ended in "expected ';'".

   And LL was not distinguished from L, so 42LL was typed long. That is
   harmless where long is 64 bits and silently narrowing where it is 32, which
   is x86_64-windows and is the whole reason that target exists. */
int main(void)
{
    double a = .5;
    double b = 5.;
    unsigned long long u = 42ULL;
    long long s = 42LL;
    unsigned long ul = 0xFFUL;
    char h = 0x20;

    if (a + b != 5.5) return 1;
    if (u != 42) return 2;
    if (s != 42) return 3;
    if (ul != 255) return 4;
    if (h != 32) return 5;
    if (sizeof(42LL) != 8) return 6;      /* long long, not long */
    if (sizeof(42ULL) != 8) return 7;
    return 0;
}
