// expect: 100
// Twice as many arguments as there are registers, so half of them travel above
// the shadow area and their order there has to be right.
int weigh(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return a * 1 + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

int main(void) { return weigh(1, 2, 3, 4, 1, 2, 3, 4); }
