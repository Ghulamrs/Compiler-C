// expect: 120
int main(void)
{
    int n = 5;
    int f = 1;
    while (n > 1) {
        f = f * n;
        n = n - 1;
    }
    return f;
}
