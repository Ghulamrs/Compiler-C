// expect: 5
int main(void) { int x = 5; int *p = &x; int **q = &p; return **q; }
