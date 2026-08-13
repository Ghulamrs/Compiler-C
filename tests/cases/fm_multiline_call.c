// expect: 12
/* A call written across three lines. The line it starts on is not enough to
   expand it. */
#define ADD3(a, b, c) ((a) + (b) + (c))
int main(void)
{
    return ADD3(3,
                4,
                5);
}
