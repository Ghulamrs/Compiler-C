// expect: 4
int main(void)
{
    int x = 4;
    int *p = &x;
    int *q = 0;
    int a = p ? *p : 0;
    int b = q ? *q : 0;
    return a + b;
}
