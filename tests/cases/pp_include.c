// expect: 30
#include "pp_helper.h"
int helper_twice(int n) { return n + n; }
int main(void)
{
#ifdef HELPER_ADDED
    return helper_twice(HELPER_LIMIT);
#else
    return 0;
#endif
}
