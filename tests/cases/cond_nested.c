// expect: 2
/* Right-associative: the else arm is the whole conditional after it. */
int main(void)
{
    int n = 0;
    return n > 0 ? 1 : n < 0 ? 3 : 2;
}
