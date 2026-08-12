// expect: 0
/* Prototypes first: fact is called above the line that defines it, and
   putchar is not defined here at all. Both are checked against these. */
int putchar(int c);
int fact(int n);

int main(void)
{
    int n = 5;
    int f = fact(n);

    putchar(48 + f / 100);       /* 120, one digit at a time */
    putchar(48 + f / 10 % 10);
    putchar(48 + f % 10);
    putchar(10);
    return 0;
}

int fact(int n)
{
    if (n <= 1) { return 1; }
    return n * fact(n - 1);      /* recursion, and a computed argument */
}
