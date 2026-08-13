// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    int n = 7;
    printf("%s\n", n % 2 ? "odd" : "even");
    printf("%d\n", n > 5 ? n * 2 : n);
    return 0;
}
