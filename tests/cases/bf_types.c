// expect: 0
int printf(char *fmt, ...);
struct T { unsigned char c : 3; unsigned short s : 9; unsigned long l : 40; };
int main(void)
{
    struct T t;
    t.c = 7; t.s = 511; t.l = 1099511627775;
    printf("%u %u %lu %d\n", t.c, t.s, t.l, (int)sizeof(struct T));
    return 0;
}
