// expect: 1
struct Point { int x; int y; };
int main(void) { struct Point p; p.x = 11; struct Point *q = &p; return q->x == (*q).x; }
