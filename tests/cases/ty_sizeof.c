// expect: 1
/* multiplication stands in for &&, which the language does not have yet */
int main(void) { return (sizeof(char) == 1) * (sizeof(short) == 2) *
                        (sizeof(int) == 4) * (sizeof(long) == 8) *
                        (sizeof(long long) == 8); }
