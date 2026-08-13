// expect: 0
/* The classic: an argument used twice is evaluated twice. Worth a test because
   it is the behaviour, not a bug - textual substitution has no other option. */
int printf(char *fmt, ...);
int calls = 0;
int bump(void) { calls = calls + 1; return 2; }
#define SQUARE(x) ((x) * (x))
int main(void)
{
    int v = SQUARE(bump());
    printf("%d %d\n", v, calls);
    return 0;
}
