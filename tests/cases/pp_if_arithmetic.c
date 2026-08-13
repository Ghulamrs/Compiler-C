// expect: 12
#define LEVEL 3
int main(void)
{
#if LEVEL * 4 == 12
    return 12;
#else
    return 0;
#endif
}
