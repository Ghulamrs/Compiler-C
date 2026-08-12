// expect: 5
int main(void) { int i; for (i = 0; i < 100; i = i + 1) { if (i == 5) break; } return i; }
