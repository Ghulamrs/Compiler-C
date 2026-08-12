// expect: 1
int main(void) { int *p; char *c; return (sizeof p == 8) * (sizeof c == 8); }
