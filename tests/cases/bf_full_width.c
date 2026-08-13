// expect: 0
int printf(char *fmt, ...);
struct W { unsigned int a : 32; int b : 32; };
int main(void)
{
    struct W w;
    w.a = 4294967295;
    w.b = -1;
    printf("%u %d %d\n", w.a, w.b, (int)sizeof(struct W));
    return 0;
}
