// expect: 0
/* An unnamed bit-field pads and declares nothing; a zero-width one starts the
   next storage unit. */
int printf(char *fmt, ...);
struct P { unsigned int a : 3; unsigned int : 2; unsigned int b : 3; };
struct Z { unsigned int a : 3; unsigned int : 0; unsigned int b : 3; };
int main(void)
{
    struct P p;
    struct Z z;
    p.a = 7; p.b = 5;
    z.a = 7; z.b = 5;
    printf("%u %u %d\n", p.a, p.b, (int)sizeof(struct P));
    printf("%u %u %d\n", z.a, z.b, (int)sizeof(struct Z));
    return 0;
}
