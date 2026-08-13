// expect: 0
int printf(char *fmt, ...);
const int base = 100;
int main(void)
{
    const int step = 7;
    int i;
    int total = 0;
    for (i = 0; i < 3; ++i) total = total + base + step;
    printf("%d %d %d\n", base, step, total);
    return 0;
}
