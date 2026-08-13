// expect: 9
struct Point { int x; int y; };
int sum(struct Point *);
int sum(struct Point *p) { return p->x + p->y; }
int main(void)
{
    struct Point pt;
    pt.x = 4;
    pt.y = 5;
    return sum(&pt);
}
