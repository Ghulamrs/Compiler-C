// expect: 21
/* a call as the argument to another call; sum(fact(3)) is sum(6),
   chosen over fact(fact(3)) because that is 720 and an exit status
   keeps only the low byte - it came back as 208 */
int fact(int n);
int sum(int n);
int fact(int n) { if (n <= 1) { return 1; } return n * fact(n - 1); }
int sum(int n) { if (n == 0) { return 0; } return n + sum(n - 1); }
int main(void) { return sum(fact(3)); }
