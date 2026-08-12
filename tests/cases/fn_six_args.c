// expect: 21
/* all six System V argument registers at once */
int six(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
int main(void) { return six(1, 2, 3, 4, 5, 6); }
