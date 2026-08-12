// expect: 1
enum E { A = 5, B, C = 20, D };
int main(void) { return (A == 5) * (B == 6) * (C == 20) * (D == 21); }
