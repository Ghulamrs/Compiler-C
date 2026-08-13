// expect: 25
/* A macro written across two lines with a backslash. */
#define SUM (10 + \
             15)
int main(void)
{
    return SUM;
}
