// expect: 0
int putchar(int c);
int stars(int n)
{
    int i = 0;
    while (i < n) { putchar(42); i = i + 1; }
    putchar(10);
    return 0;
}
int main(void)
{
    int r = 1;
    while (r <= 5) { stars(r); r = r + 1; }
    return 0;
}
