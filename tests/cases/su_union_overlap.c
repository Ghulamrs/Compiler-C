// expect: 1
/* every member starts at the same place */
union U { int i; char bytes[4]; };
int main(void) { union U u; u.i = 0; u.bytes[0] = 1; return u.i == 1; }
