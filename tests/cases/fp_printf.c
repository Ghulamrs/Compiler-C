// expect: 0
/* printf with a floating argument. This is what finally exercises the 16-byte
   stack alignment: libc uses an aligned SSE store to save the register area,
   and a misaligned %rsp faults there. */
int printf(char *fmt, ...);
int main(void) { printf("%.2f\n", 1.5); return 0; }
