// expect: 0
int putchar(int c);
int main(void)
{
    int i = 0;
    while (i < 10) { putchar(48 + i); i = i + 1; }
    putchar(10);
    return 0;
}
