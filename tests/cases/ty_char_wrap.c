// expect: 1
int main(void) { signed char c = 127; c = c + 1; return c == -128; }
