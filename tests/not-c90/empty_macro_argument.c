/* An empty macro argument is C99. C90 requires each argument to have at least
   one preprocessing token. */
#define PAIR(a, b) (a + b)
int main(void)
{
    return PAIR(, 0);
}
