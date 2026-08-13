// expect: 0
/* A macro must not rewrite the inside of a string, a character constant or a
   comment. n is defined here precisely because the text below is full of them. */
int printf(char *fmt, ...);
#define n 99
int main(void)
{
    /* n stays n in this comment */
    char c = 'n';
    printf("n is not %d here\n", n);
    printf("%c\n", c);
    return 0;
}
