// expect: 0
/* A type with no name in it - which a cast and a sizeof both need. */
int printf(char *, ...);
int main(void)
{
    printf("%d %d %d\n", (int)sizeof(char[8]), (int)sizeof(int (*)[4]),
           (int)sizeof(int *[4]));
    return 0;
}
