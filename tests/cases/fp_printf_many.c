// expect: 0
int printf(char *fmt, ...);
int main(void) { printf("%.1f %.1f %.1f %.1f\n", 1.5, 2.5, 3.5, 4.5); return 0; }
