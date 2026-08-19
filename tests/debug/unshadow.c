// The other half of the shadowing question: once the block has ended, the
// outer name is the one in scope again.
//
// A lexical block that opened correctly and never closed would pass the
// inner check in scopes.c and fail this one, which is why both are here. The
// block's high_pc is what this reads, and nothing else tests it.
//
// stop: 23
// print: n 3
// print: total 41
// print: kept 20

int outerName(void)
{
    int n = 3;
    int total = 0;
    int kept = 0;
    {
        int n = 20;
        kept = n;
        total = n + 21;
    }
    return total + n;
}

int main(void)
{
    return outerName();
}
