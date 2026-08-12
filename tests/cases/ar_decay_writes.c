// expect: 42
int fill(int *p);
int fill(int *p) { p[1] = 42; return 0; }
int main(void) { int a[3]; fill(a); return a[1]; }
