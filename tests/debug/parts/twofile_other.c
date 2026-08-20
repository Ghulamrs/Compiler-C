// The second half of tests/debug/twofile.c. It is here rather than beside the
// cases so that the suite's own loop does not pick it up as a case of its own:
// it has no main, and nothing to say on its own.

int twice(int n)
{
    int doubled = n + n;
    return doubled;
}
