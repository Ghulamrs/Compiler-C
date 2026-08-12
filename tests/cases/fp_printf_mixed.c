// expect: 0
int printf(char *fmt, ...);
int main(void) { printf("%d %.3f %s\n", 42, 2.5, "ok"); return 0; }
