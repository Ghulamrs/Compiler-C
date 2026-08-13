// expect: 5
int main(void)
{
    int r = 0;
    switch (2) {
    case 1: r = 1; break;
    case 2: goto after;
    default: r = 3;
    }
    r = 99;
after:
    return r + 5;
}
