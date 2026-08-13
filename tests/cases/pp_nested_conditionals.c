// expect: 7
#define OUTER
#define INNER 1
int main(void)
{
#ifdef OUTER
#  if INNER
    return 7;
#  else
    return 8;
#  endif
#else
#  ifdef ANYTHING
    return 9;
#  endif
    return 10;
#endif
}
