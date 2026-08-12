// expect: 45
int main(void) { int a[10]; int i = 0;
                 while (i < 10) { a[i] = i; i = i + 1; }
                 int sum = 0; i = 0;
                 while (i < 10) { sum = sum + a[i]; i = i + 1; }
                 return sum; }
