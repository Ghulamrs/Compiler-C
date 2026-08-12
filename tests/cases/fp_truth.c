// expect: 1
int main(void) { double zero = 0.0; double one = 1.5;
                 int n = 0;
                 if (one) { n = n + 1; }
                 if (zero) { n = n + 10; }
                 return (n == 1) * (!zero) * (!one == 0); }
