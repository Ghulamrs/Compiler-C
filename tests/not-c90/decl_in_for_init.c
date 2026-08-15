/* A declaration in the first clause of a 'for' is C99. In C90 the name has to
   exist before the loop, and it outlives it. */
int main(void)
{
    int total = 0;
    for (int i = 0; i < 3; i = i + 1) total = total + i;
    return total - 3;
}
