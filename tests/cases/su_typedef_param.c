// expect: 12
typedef struct Pair { int a; int b; } Pair;
int total(Pair *p);
int total(Pair *p) { return p->a + p->b; }
int main(void) { Pair p; p.a = 5; p.b = 7; return total(&p); }
