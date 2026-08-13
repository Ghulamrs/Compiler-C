// expect: 10
#define FIRST_PLUS(a, ...) ((a) + add2(__VA_ARGS__))
int add2(int x, int y) { return x + y; }
int main(void)
{
    return FIRST_PLUS(1, 4, 5);
}
