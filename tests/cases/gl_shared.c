// expect: 11
int counter;
int bump(void);
int bump(void) { counter = counter + 1; return counter; }
int main(void) { counter = 10; return bump(); }
