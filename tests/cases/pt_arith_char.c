// expect: 1
int main(void) { char a[4]; char *p = a; long d = (long)(p + 1) - (long)p; return d == 1; }
