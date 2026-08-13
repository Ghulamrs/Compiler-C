// expect: 0
int printf(char *fmt, ...);
union U { unsigned int a : 4; unsigned int b : 8; };
int main(void)
{
    union U u;
    u.b = 255;
    printf("%u %u %d\n", u.a, u.b, (int)sizeof(union U));
    return 0;
}
