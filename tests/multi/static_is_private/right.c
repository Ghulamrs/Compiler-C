static int counter = 100;
static int step(void) { counter = counter + 10; return counter; }
int right_twice(void) { step(); return step(); }
