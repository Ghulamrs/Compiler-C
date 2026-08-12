// expect: 1
int main(void) { double a[3]; a[0] = 1.5; a[1] = 2.5; a[2] = a[0] + a[1];
                 return (a[2] == 4.0) * (sizeof a == 24); }
