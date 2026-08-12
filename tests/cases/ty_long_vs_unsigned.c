// expect: 1
/* on LP64 long holds every unsigned int, so u becomes signed and -1 < 1 */
int main(void) { unsigned int u = 1; long l = -1; return l < u; }
