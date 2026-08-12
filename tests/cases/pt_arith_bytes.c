// expect: 1
/* p + 1 moves by sizeof(int), not by one byte */
int main(void) { int a[4]; int *p = a; long d = (long)(p + 1) - (long)p; return d == 4; }
