// expect: 1
union U { int i; long l; };
int main(void) { union U u; return (char *)&u.i == (char *)&u.l; }
