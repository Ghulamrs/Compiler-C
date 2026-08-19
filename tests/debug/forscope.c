// A 'for' whose init declares something is a scope with no block of its own,
// and the body inside it is a second one. Both have to be described, or the
// loop counter and the outer name of the same spelling collapse into one.
//
// The counter starts at 5 rather than 0 so that the first time the breakpoint
// is hit every expected value below is a different number - the suite answers
// all of one case's prints in a single run, and two expectations sharing a
// value could cover for each other.
//
// stop: 22
// print: i 5
// print: sq 15
// print: sum 0

int counted(void)
{
    int i = 900;
    int sum = 0;

    for (int i = 5; i < 9; i++) {
        int sq = i * 3;
        sum += sq;
    }
    return sum + i;
}

int main(void)
{
    return counted() - 900;
}
