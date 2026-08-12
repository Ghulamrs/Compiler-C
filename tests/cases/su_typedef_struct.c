// expect: 7
typedef struct Point { int x; int y; } Point;
int main(void) { Point p; p.x = 3; p.y = 4; return p.x + p.y; }
