// expect: 55
/* default need not come last, and is only reached when nothing matched. */
int main(void)
{
    int r = 0;
    switch (100) {
    case 1: r = 1; break;
    default: r = 55; break;
    case 2: r = 2; break;
    }
    return r;
}
