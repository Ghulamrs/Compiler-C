// expect: 0
/* Writing one field must leave the others alone. A store that wrote the whole
   storage unit would pass the first check and fail every later one. */
int printf(char *fmt, ...);
struct F { unsigned int a : 4; unsigned int b : 4; unsigned int c : 4; };
int main(void)
{
    struct F f;
    f.a = 1; f.b = 2; f.c = 3;
    printf("%u %u %u\n", f.a, f.b, f.c);
    f.b = 15;
    printf("%u %u %u\n", f.a, f.b, f.c);
    f.a = 0;
    printf("%u %u %u\n", f.a, f.b, f.c);
    return 0;
}
