// expect: 1
typedef long Big;
typedef struct S { char a; long b; } S;
int main(void) { return (sizeof(Big) == 8) * (sizeof(S) == 16); }
