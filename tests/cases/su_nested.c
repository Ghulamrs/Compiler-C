// expect: 30
struct Inner { int a; int b; };
struct Outer { struct Inner in; int c; };
int main(void) { struct Outer o; o.in.a = 10; o.in.b = 15; o.c = 5;
                 return o.in.a + o.in.b + o.c; }
