// expect: 1
int mix(char a, short b, int c, long d);
int mix(char a, short b, int c, long d)
{ return (a == -1) * (b == -300) * (c == 70000) * (d == 5000000000); }
int main(void) { return mix(-1, -300, 70000, 5000000000); }
