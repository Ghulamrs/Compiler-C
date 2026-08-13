// expect: 10
int main(void)
{
    int i = 0;
    int r = 0;
top:
    r = r + i;
    i = i + 1;
    if (i < 5) goto top;
    return r;
}
