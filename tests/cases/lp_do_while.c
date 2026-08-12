// expect: 1
/* the body runs before the condition is asked */
int main(void) { int n = 0; do { n = n + 1; } while (0); return n == 1; }
