// expect: 44
/* the result converts back to char, as a plain assignment would */
int main(void) { char c = 0; c += 300; return c; }
