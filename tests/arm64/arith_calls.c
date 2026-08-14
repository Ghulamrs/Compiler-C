// expect: 69
// Recursion, a for loop, and a call - the first program this backend ran.
int add(int a, int b) { return a + b; }
int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }
int main(void) {
    int i;
    int s;
    s = 0;
    for (i = 0; i < 10; i = i + 1) s = s + i;
    return add(s, fact(4)) % 100;
}
