// expect: 0
// Apple passes variadic arguments on the stack rather than in registers, and
// this is the case that proved it: putting them in x0-x7 as AAPCS64 says
// printed 1809625552 and -1899641628 where 42 and 7 were meant.
int printf(char *fmt, ...);
int sq(int n) { return n * n; }
int main(void) {
    int i;
    printf("n=%d m=%d\n", 42, 7);
    printf("one=%d\n", 1);
    printf("plain\n");
    for (i = 1; i <= 3; i = i + 1) printf("sq(%d)=%d\n", i, sq(i));
    printf("many=%d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6);
    return 0;
}
