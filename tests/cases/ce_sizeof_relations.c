// expect: 0
int printf(char *fmt, ...);
int lp64 = sizeof(long) == 8 && sizeof(int) == 4;
int slots = (sizeof(long) * 8) / 16;
int main(void)
{
    printf("%d %d %d\n", lp64, slots, 1 ? 10 % 3 : 99);
    return 0;
}
