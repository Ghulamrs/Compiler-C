// expect: 25
int main(void)
{
    int a = 5;
    int b = 3;
    int max = a > b ? a : b;
    return max * (a > b ? 5 : 1);
}
