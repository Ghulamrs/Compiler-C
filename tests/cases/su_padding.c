// expect: 1
/* char then int: three bytes of padding, and the whole rounded to 8 */
struct Mixed { char c; int i; };
int main(void) { return sizeof(struct Mixed) == 8; }
