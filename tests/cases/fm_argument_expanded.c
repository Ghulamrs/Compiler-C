// expect: 25
/* The argument is expanded before it is substituted, so a macro may be passed
   to a macro. */
#define N 5
#define SQUARE(x) ((x) * (x))
int main(void)
{
    return SQUARE(N);
}
