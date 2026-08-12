// expect: 1
double half(double x);
double half(double x) { return x / 2.0; }
int main(void) { return half(5.0) == 2.5; }
