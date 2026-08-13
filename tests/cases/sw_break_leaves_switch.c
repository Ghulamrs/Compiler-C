// expect: 9
/* break inside a switch leaves the switch, not the loop around it. */
int main(void)
{
    int i;
    int r = 0;
    for (i = 0; i < 4; ++i) {
        switch (i) {
        case 0: r = r + 1; break;
        case 1: r = r + 2; break;
        default: r = r + 3; break;
        }
    }
    return r;
}
