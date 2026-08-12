// expect: 1
struct Rec { int n; double d; };
int main(void) { struct Rec r; r.n = 3; r.d = 2.5;
                 return (r.d * r.n == 7.5) * (sizeof r == 16); }
