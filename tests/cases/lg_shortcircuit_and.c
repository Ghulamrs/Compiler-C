// expect: 0
/* prints B only. If && evaluated its right side, an A would appear first. */
int putchar(int c);
int main(void) { int zero = 0; zero && putchar(65); putchar(66); return 0; }
