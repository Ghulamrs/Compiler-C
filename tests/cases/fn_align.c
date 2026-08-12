// expect: 7
/* the call happens with one value already pushed, so %rsp needs correcting */
int f(int x);
int main(void) { return 1 + f(2) * 2; }
int f(int x) { return x + 1; }
