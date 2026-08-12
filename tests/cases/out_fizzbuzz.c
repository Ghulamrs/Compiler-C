// expect: 0
int putchar(int c);
int print_num(int n)
{
    if (n >= 10) { putchar(48 + n / 10); }
    putchar(48 + n % 10);
    return 0;
}
int main(void)
{
    int i = 1;
    while (i <= 15) {
        if (i % 15 == 0) { putchar(70); putchar(66); }
        else { if (i % 3 == 0) { putchar(70); }
               else { if (i % 5 == 0) { putchar(66); }
                      else { print_num(i); } } }
        putchar(10);
        i = i + 1;
    }
    return 0;
}
