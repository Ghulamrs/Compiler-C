// expect: 12
int setit(int *p);
int setit(int *p) { *p = 12; return 0; }
int main(void) { int x = 0; setit(&x); return x; }
