// expect: 1
/* integer division truncates; floating division does not */
int main(void) { int a = 7; int b = 2; double x = 7.0; double y = 2.0;
                 return (a / b == 3) * (x / y == 3.5); }
