// expect: 0
/* Layout, measured. Every one of these is a claim about the ABI that gcc
   settles. */
int printf(char *fmt, ...);
struct A { unsigned int a : 1; };
struct B { unsigned int a : 3; unsigned int b : 5; };
struct C { unsigned int a : 30; unsigned int b : 5; };
struct D { unsigned char a : 3; unsigned char b : 6; };
struct E { unsigned int a : 32; };
int main(void)
{
    printf("%d %d %d\n", (int)sizeof(struct A), (int)sizeof(struct B), (int)sizeof(struct C));
    printf("%d %d\n", (int)sizeof(struct D), (int)sizeof(struct E));
    return 0;
}
