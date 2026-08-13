// expect: 3
/* One argument, not two: the comma is inside brackets. */
#define FIRST(x) (x)
int add3(int a, int b, int c) { return a + b + c; }
int main(void)
{
    return FIRST(add3(1, 1, 1));
}
