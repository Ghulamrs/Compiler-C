// expect: 30
/* '##' joins its two sides into one name before anything else looks at it. */
#define JOIN(a, b) a ## b
int value10 = 10;
int value20 = 20;
int main(void)
{
    return JOIN(value, 10) + JOIN(value, 20);
}
