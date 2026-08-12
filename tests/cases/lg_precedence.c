// expect: 1
/* && binds tighter than ||, so this is 1 || (0 && 0) */
int main(void) { return 1 || 0 && 0; }
