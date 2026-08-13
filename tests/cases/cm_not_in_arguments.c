// expect: 0
/* The commas here separate arguments and are not operators. If expr() were
   used for an argument, this would be one argument and not three. */
int printf(char *fmt, ...);
int three(int a, int b, int c) { return a * 100 + b * 10 + c; }
int main(void)
{
    printf("%d\n", three(1, 2, 3));
    printf("%d\n", three((1, 2), 3, 4));   /* now one argument, deliberately */
    return 0;
}
