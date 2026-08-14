// expect: 55
// Recursion, so the shadow area is opened and closed at every level and the
// frame has to come back exactly as it went in.
int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }

int main(void) { return fib(10); }
