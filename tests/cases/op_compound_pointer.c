// expect: 30
int main(void) { int a[4]; a[0] = 10; a[1] = 20; a[2] = 30;
                 int *p = a; p += 2; return *p; }
