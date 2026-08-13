// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    int i;
    for (i = 0; i < 3; ++i) {
        static int calls = 0;
        calls = calls + 1;
        printf("%d\n", calls);
    }
    return 0;
}
