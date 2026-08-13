// expect: 7
/* An empty body expands to nothing, which is how a macro used as a marker
   disappears. */
#define NOTHING
int main(void)
{
    int n = NOTHING 7;
    return n NOTHING;
}
