// expect: 42
/* volatile is accepted and changes nothing, which is honest here: every value
   goes to memory and comes back on every access, so there is no caching for it
   to forbid. */
volatile int flag = 42;
int main(void)
{
    volatile int local = 0;
    local = flag;
    return local;
}
