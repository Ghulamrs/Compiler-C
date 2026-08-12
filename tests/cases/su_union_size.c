// expect: 1
union U { char c; int i; long l; };
int main(void) { return sizeof(union U) == 8; }
