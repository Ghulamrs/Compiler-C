// expect: 9
int g, h = 4, i = 5;
static int j, k = 0;
int main(void)
{
    g = h + i;
    return g + j + k;
}
