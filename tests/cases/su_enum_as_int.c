// expect: 1
enum Flag { Off, On };
int main(void) { enum Flag f = On; return (f == 1) * (sizeof(enum Flag) == 4); }
