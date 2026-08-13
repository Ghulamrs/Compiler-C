// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    char *s = 0;
    char *t = "text";
    printf("%s %s\n", s ? s : "null", t ? t : "null");
    return 0;
}
