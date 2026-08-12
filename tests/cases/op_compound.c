// expect: 1
int main(void) { int a = 10;
                 a += 5;  int t1 = a == 15;
                 a -= 3;  int t2 = a == 12;
                 a *= 2;  int t3 = a == 24;
                 a /= 4;  int t4 = a == 6;
                 a %= 4;  int t5 = a == 2;
                 return t1 * t2 * t3 * t4 * t5; }
