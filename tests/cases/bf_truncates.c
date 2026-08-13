// expect: 0
int printf(char *fmt, ...);
struct F { unsigned int a : 3; };
int main(void)
{
    struct F f;
    printf("%u\n", (unsigned int)(f.a = 300));   /* the value of the assignment */
    printf("%u\n", f.a);                          /* and what is actually there */
    return 0;
}
