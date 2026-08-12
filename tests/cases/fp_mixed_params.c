// expect: 1
/* the two lanes are counted separately: 1 and 2 in the integer registers,
   1.5 and 2.5 in the SSE ones */
double mix(int a, double b, int c, double d);
double mix(int a, double b, int c, double d) { return a + b + c + d; }
int main(void) { return mix(1, 1.5, 2, 2.5) == 7.0; }
