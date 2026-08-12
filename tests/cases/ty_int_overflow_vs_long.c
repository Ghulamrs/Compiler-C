// expect: 1
/* the int multiply wraps at 32 bits; the long one does not */
int main(void) { int i = 100000; long l = 100000; return (i * i) != (l * l); }
