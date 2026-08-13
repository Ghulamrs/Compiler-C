// expect: 0
/* '#__VA_ARGS__' is the whole variable part as it was written, commas and all. */
int printf(char *fmt, ...);
#define SHOW(...) #__VA_ARGS__
int main(void)
{
    printf("%s\n", SHOW(a, b, c));
    printf("%s\n", SHOW(1 + 2));
    return 0;
}
