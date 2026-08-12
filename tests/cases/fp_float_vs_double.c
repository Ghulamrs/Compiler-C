// expect: 1
/* float promotes to double in a mixed expression */
int main(void) { float f = 0.5f; double d = 0.25; return (f + d) == 0.75; }
