// expect: 1
int main(void) { double a = 1.5; double b = 2.5;
                 return (a < b) * (b > a) * (a <= 1.5) * (b >= 2.5) * (a != b); }
