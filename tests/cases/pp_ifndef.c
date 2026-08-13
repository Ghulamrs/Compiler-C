// expect: 2
int main(void)
{
#ifndef ABSENT
    return 2;
#else
    return 3;
#endif
}
