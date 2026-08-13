// expect: 0
/* Bit-fields beside ordinary members: the cursor has to return to a byte
   boundary before the int, and to its alignment after that. */
int printf(char *fmt, ...);
struct M { unsigned int a : 3; int n; unsigned int b : 5; };
int main(void)
{
    struct M m;
    m.a = 7; m.n = 42; m.b = 31;
    printf("%u %d %u %d\n", m.a, m.n, m.b, (int)sizeof(struct M));
    return 0;
}
