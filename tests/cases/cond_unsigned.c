// expect: 1
/* The usual arithmetic conversions apply to the arms, so the result is
   unsigned and the comparison against -1 is the unsigned one. */
int main(void)
{
    int n = 1;
    unsigned int u = 1u;
    return (n ? u : -1) == 1u;
}
