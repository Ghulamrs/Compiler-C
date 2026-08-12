// expect: 1
/* the int converts to double, so this is 2.5 and not 2 */
int main(void) { int i = 2; double d = 0.5; return (i + d) == 2.5; }
