// expect: 0
/* and when it is not short circuited, the right side does run: prints AB */
int putchar(int c);
int main(void) { int one = 1; one && putchar(65); putchar(66); return 0; }
