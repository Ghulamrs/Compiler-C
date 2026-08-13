// expect: 0
/* '#' takes the argument as it was written, not as it would expand. */
int printf(char *fmt, ...);
#define N 5
#define NAME(x) #x
int main(void)
{
    printf("%s\n", NAME(N));
    printf("%s\n", NAME(a + b));
    return 0;
}
