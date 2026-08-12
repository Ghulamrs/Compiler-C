// expect: 0
/* prints C: the first && stops, so neither putchar in the chain runs */
int putchar(int c);
int main(void) { int zero = 0; zero && putchar(65) && putchar(66); putchar(67); return 0; }
