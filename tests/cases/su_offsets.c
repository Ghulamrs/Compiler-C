// expect: 1
struct Mixed { char c; int i; long l; };
int main(void) { struct Mixed m; char *base = (char *)&m;
                 return ((char *)&m.c - base == 0) * ((char *)&m.i - base == 4)
                      * ((char *)&m.l - base == 8) * (sizeof m == 16); }
