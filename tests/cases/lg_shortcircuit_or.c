// expect: 0
/* prints B only. || must stop once the left side is true. */
int putchar(int c);
int main(void) { int one = 1; one || putchar(65); putchar(66); return 0; }
