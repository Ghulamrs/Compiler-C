// expect: 0
/* Static storage with block scope. The two n are different objects that happen
   to share a name, and *both* outlive the call - which is the thing that makes
   this worth a test: the inner one keeps counting on the second call too. */
int printf(char *fmt, ...);
int f(void)
{
    static int n = 5;
    {
        static int n = 2;
        n = n + 1;
        printf("inner=%d ", n);
    }
    n = n + 10;
    printf("outer=%d\n", n);
    return n;
}
int main(void)
{
    f();
    f();
    return 0;
}
