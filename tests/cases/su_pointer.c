// expect: 9
struct Point { int x; int y; };
int move(struct Point *p);
int move(struct Point *p) { p->x = 4; p->y = 5; return 0; }
int main(void) { struct Point p; move(&p); return p.x + p.y; }
