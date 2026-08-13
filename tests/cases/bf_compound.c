// expect: 0
/* Compound assignment and prefix ++ on a bit-field, which the parser lowers to
   a read and a write of the same field - and which wrap at the field's width. */
int printf(char *fmt, ...);
struct F { unsigned int a : 3; unsigned int b : 4; };
int main(void)
{
    struct F f;
    f.a = 1; f.b = 0;
    f.a += 3;
    ++f.b;
    printf("%u %u\n", f.a, f.b);
    f.a = 7;
    ++f.a;                       /* wraps to 0 within three bits */
    printf("%u\n", f.a);
    return 0;
}
