// expect: 0
int putchar(int c);
int main(void)
{
    char *s = "abcde";
    while (*s) { putchar(*s); s = s + 1; }
    putchar(10);
    return 0;
}
