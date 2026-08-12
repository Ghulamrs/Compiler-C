// expect: 1
int main(void) { double d = 1.25; double *p = &d; *p = 2.5; return d == 2.5; }
