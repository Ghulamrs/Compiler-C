// expect: 0
/* The form a real header uses. */
int printf(char *, ...);
int putchar(int);
int main(void)
{
    printf("%d %s\n", 42, "text");
    putchar('h');
    putchar('i');
    putchar('\n');
    return 0;
}
