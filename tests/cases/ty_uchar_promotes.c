// expect: 1
/* unsigned char promotes to int, not to unsigned int, so this stays signed */
int main(void) { unsigned char c = 255; int i = -1; return i < c; }
