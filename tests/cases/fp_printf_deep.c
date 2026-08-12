// expect: 0
/* the call happens with values already on the stack, so %rsp is odd and the
   padding is what keeps it aligned */
int printf(char *fmt, ...);
int one(void);
int one(void) { return 1; }
int main(void) { int n = one() + one() * printf("%.1f\n", 9.5); return 0; }
