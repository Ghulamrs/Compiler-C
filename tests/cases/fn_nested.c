// expect: 30
int add(int a, int b);
int mul(int a, int b);
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int main(void) { return add(mul(2, 3), mul(4, 6)); }
