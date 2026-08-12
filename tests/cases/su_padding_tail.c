// expect: 1
/* trailing padding is part of sizeof, so an array of these stays aligned */
struct Tail { int i; char c; };
int main(void) { return sizeof(struct Tail) == 8; }
