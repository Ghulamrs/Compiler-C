// expect: 1
struct P { int x; int y; };
int main(void) { struct P p; p.x = 1; p.y = 2; p.x += 10; p.y *= 5;
                 return (p.x == 11) * (p.y == 10); }
