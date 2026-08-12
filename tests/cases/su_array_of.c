// expect: 1
struct Point { int x; int y; };
int main(void) { struct Point a[4]; int i = 0;
                 while (i < 4) { a[i].x = i; a[i].y = i * 10; i = i + 1; }
                 return (a[3].y == 30) * (a[0].x == 0) * (sizeof a == 32); }
