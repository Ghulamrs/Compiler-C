// expect: 0
/* b cannot fit in what is left of the first int, so it starts a new one and the
   gap is padding. The values prove the two do not overlap. */
int printf(char *fmt, ...);
struct S { unsigned int a : 30; unsigned int b : 5; };
int main(void)
{
    struct S s;
    s.a = 1073741823;
    s.b = 31;
    printf("%u %u %d\n", s.a, s.b, (int)sizeof(struct S));
    return 0;
}
