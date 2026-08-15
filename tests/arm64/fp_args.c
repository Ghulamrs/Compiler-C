// expect: 0
/* The two register files are counted independently under AAPCS64, so a named
   floating argument takes the next d register rather than the one at its
   position. mix(1, 1.5, 2, 2.5) puts 1 and 2 in x0 and x1 while 1.5 and 2.5 go
   in d0 and d1 - a backend assigning by position would put 1.5 in x1.

   Apple's variadic rule is the opposite and applies past the '...': those go on
   the stack in eight-byte slots, in registers never. printf is the proof. */
int printf(const char *, ...);

double mix(int a, double b, int c, double d)
{
    printf("a=%d b=%.1f c=%d d=%.1f\n", a, b, c, d);
    return b + d;
}

float single(float f, float g) { return f + g; }

int main(void)
{
    double r = mix(1, 1.5, 2, 2.5);
    printf("mix returned %.1f\n", r);
    printf("single returned %.2f\n", (double)single(0.25f, 0.5f));
    return 0;
}
