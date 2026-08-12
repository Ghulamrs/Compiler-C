// expect: 0
/* i converts to unsigned and becomes 4294967295 */
int main(void) { unsigned int u = 1; int i = -1; return i < u; }
