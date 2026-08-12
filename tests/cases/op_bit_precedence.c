// expect: 1
/* & binds looser than ==, which is C and surprises everyone */
int main(void) { return (1 & 1 == 1) == (1 & (1 == 1)); }
