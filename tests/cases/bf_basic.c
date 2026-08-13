// expect: 13
struct F { unsigned int a : 3; unsigned int b : 5; };
int main(void)
{
    struct F f;
    f.a = 5;
    f.b = 8;
    return f.a + f.b;
}
