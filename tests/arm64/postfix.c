// expect: 0
/* Postfix keeps three things in flight - the address, the old value that is the
   expression's result, and the new value that gets stored. It is a node rather
   than '(x += 1) - 1' because that rewrite is wrong wherever the type wraps:
   an unsigned char at 255 yields 255 and stores 0, where the rewrite computes
   0 - 1. */
int printf(const char *, ...);

int main(void)
{
    int i = 5;
    unsigned char c = 255;
    double d = 1.5;
    int a[3];
    int *p = a;

    printf("i++ gives %d, i is now %d\n", i++, i);
    printf("i-- gives %d, i is now %d\n", i--, i);
    printf("c++ gives %d, c is now %d\n", c++, c);
    printf("d++ gives %.1f, d is now %.1f\n", d++, d);

    a[0] = 10; a[1] = 20; a[2] = 30;
    printf("*p++ gives %d, p now points at %d\n", *p++, *p);
    return 0;
}
