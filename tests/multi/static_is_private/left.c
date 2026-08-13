/* Both units define a "counter" and a "step". If static did not give them
   internal linkage the linker would refuse this with a duplicate symbol, so
   the fact that it links at all is the assertion. */
static int counter = 0;
static int step(void) { counter = counter + 1; return counter; }
int left_twice(void) { step(); return step(); }
