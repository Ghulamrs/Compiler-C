// expect: 1
int big(long a, long b);
int big(long a, long b) { return a * b == 10000000000; }
int main(void) { return big(100000, 100000); }
