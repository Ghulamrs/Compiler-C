// expect: 0
/* The idiom the feature exists for. */
int printf(char *fmt, ...);
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
int main(void)
{
    LOG("%d %d\n", 1, 2);
    LOG("%d %d %d\n", 3, 4, 5);
    return 0;
}
