// expect: 44
int main(void) { char c = 0; char *p = &c; *p = 300; return c; }
