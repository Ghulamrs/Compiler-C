// expect: 1
/* whole-object assignment copies; changing the source must not touch the copy */
struct Point { int x; int y; };
int main(void) { struct Point a; a.x = 1; a.y = 2;
                 struct Point b; b = a;
                 a.x = 99;
                 return (b.x == 1) * (b.y == 2) * (a.x == 99); }
