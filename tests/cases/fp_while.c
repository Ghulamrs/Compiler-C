// expect: 1
int main(void) { double x = 1.0; int n = 0;
                 while (x < 100.0) { x = x * 2.0; n = n + 1; }
                 return n == 7; }
