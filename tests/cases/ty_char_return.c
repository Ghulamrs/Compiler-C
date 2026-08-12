// expect: 44
char narrow(int i);
char narrow(int i) { return i; }
int main(void) { return narrow(300); }
