// expect: 1
struct Point { int x; int y; };
int main(void) { return sizeof(struct Point) == 8; }
