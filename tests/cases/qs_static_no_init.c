// expect: 1
/* Without an initialiser it starts at zero, like any object with static
   storage duration. */
int seen(void)
{
    static int flag;
    if (flag) return 0;
    flag = 1;
    return 1;
}
int main(void)
{
    seen();
    return seen() == 0;
}
