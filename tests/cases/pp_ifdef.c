// expect: 1
#define ENABLED
int main(void)
{
#ifdef ENABLED
    return 1;
#else
    return 2;
#endif
}
