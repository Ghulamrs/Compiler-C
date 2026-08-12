// expect: 1
/* the pointer exists before the type is complete */
struct Later;
int main(void) { struct Later *p = 0; return p == 0; }
