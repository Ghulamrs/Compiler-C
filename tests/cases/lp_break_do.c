// expect: 4
int main(void) { int i = 0; do { i = i + 1; if (i == 4) break; } while (i < 100); return i; }
