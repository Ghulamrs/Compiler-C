// expect: 12
/* the prototype is what lets twice() be called above its definition */
int twice(int x);
int main(void) { return twice(6); }
int twice(int x) { return x * 2; }
