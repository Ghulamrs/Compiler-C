// expect: 1
int main(void) { int a[2][3]; a[1][2] = 9; return (a[1][2] == 9) * (sizeof a == 24); }
