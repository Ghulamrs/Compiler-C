// expect: 1
double pi(void);
double pi(void) { return 3.25; }
int main(void) { return pi() == 3.25; }
