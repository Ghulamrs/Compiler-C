// expect: 40
int main(void)
{
    int a[2 * 5];
    char s[8 + 2];
    return sizeof(a) + sizeof(s) - 10;
}
