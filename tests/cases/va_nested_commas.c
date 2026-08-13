// expect: 3
/* A comma inside brackets belongs to the inner call, so this is one variable
   argument and not two. */
int add3(int a, int b, int c) { return a + b + c; }
#define ONE(...) (__VA_ARGS__)
int main(void)
{
    return ONE(add3(1, 1, 1));
}
