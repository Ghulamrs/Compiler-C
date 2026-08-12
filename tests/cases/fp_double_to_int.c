// expect: 1
/* conversion truncates towards zero, it does not round */
int main(void) { double a = 1.9; double b = -1.9; return ((int)a == 1) * ((int)b == -1); }
