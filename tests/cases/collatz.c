// expect: 111
/* how many steps does 27 take to reach 1? */
int main(void)
{
    int n = 27;
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) { n = n / 2; } else { n = 3 * n + 1; }
        steps = steps + 1;
    }
    return steps;
}
