// expect: 8
/* an array parameter is a pointer, so sizeof inside gives 8, not 16 */
int inside(char s[16]);
int inside(char s[16]) { return sizeof s; }
int main(void) { char b[16]; return inside(b); }
