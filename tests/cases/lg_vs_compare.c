// expect: 1
/* comparison binds tighter than && */
int main(void) { int a = 5; return a > 1 && a < 10; }
