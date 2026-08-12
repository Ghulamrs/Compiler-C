// expect: 1
double power(double base, int n);
double power(double base, int n) { if (n == 0) { return 1.0; } return base * power(base, n - 1); }
int main(void) { return power(2.0, 10) == 1024.0; }
