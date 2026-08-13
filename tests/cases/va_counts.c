// expect: 6
/* The variable arguments keep the commas that separated them, which is what
   makes this a call with three of them. */
int add3(int a, int b, int c) { return a + b + c; }
#define CALL(...) add3(__VA_ARGS__)
int main(void)
{
    return CALL(1, 2, 3);
}
