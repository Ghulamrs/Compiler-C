// expect: 1
int main(void) { int a = 12;
                 a &= 10; int t1 = a == 8;
                 a |= 3;  int t2 = a == 11;
                 a ^= 1;  int t3 = a == 10;
                 a <<= 2; int t4 = a == 40;
                 a >>= 3; int t5 = a == 5;
                 return t1 * t2 * t3 * t4 * t5; }
