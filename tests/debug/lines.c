// Statements on lines a debugger has to be able to name, and nothing else:
// no loop, no branch, so which line an instruction belongs to is never a
// matter of opinion.
//
// stop: 19
// func: square 12
// step: 19 20
// bt: square main

int square(int n)
{
    int r = n * n;
    return r;
}

int main(void)
{
    int total = 0;
    total = total + square(2);
    total = total + square(3);
    return total;
}
