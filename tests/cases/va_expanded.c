// expect: 0
/* The variable arguments are expanded like any other argument. */
int printf(char *fmt, ...);
#define N 5
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
int main(void)
{
    LOG("%d\n", N);
    return 0;
}
