// expect: 1
int main(void) { int a[4]; int *p = a; int *q = p + 2; return (q - p) == 2; }
