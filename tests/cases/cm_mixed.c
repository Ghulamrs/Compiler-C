// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    int i, j, sum = 0, prod = 1;
    for (i = 1, j = 5; i <= 3; ++i, --j) {
        sum += i, prod *= i;
    }
    printf("sum=%d prod=%d j=%d\n", sum, prod, j);
    return 0;
}
