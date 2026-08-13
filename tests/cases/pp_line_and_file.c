// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    printf("%d\n", __LINE__);
    printf("%d\n", __LINE__);
    return 0;
}
