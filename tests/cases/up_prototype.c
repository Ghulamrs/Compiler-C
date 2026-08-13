// expect: 7
/* A prototype may name only types, which is how headers are written. */
int add(int, int);
int add(int a, int b) { return a + b; }
int main(void)
{
    return add(3, 4);
}
