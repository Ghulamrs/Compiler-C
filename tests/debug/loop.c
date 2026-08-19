// The shapes where a line is reached more than once, and out of order: a
// loop's step runs after its body, and an else arm is not where the 'if'
// was written.
//
// stop: 23
// func: pick 12
// step: 23 22
// bt: pick main

int pick(int n)
{
    if (n > 2)
        return n * 10;
    else
        return n;
}

int main(void)
{
    int total = 0;
    int i;
    for (i = 1; i <= 4; i++)
        total = total + pick(i);
    return total;
}
