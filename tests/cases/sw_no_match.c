// expect: 7
/* Nothing matches and there is no default, so the whole statement is skipped. */
int main(void)
{
    int r = 7;
    switch (9) {
    case 1: r = 1; break;
    case 2: r = 2; break;
    }
    return r;
}
