// expect: 3
/* The ordinary goto is a forward one, which is why a label cannot be resolved
   where it is written. */
int main(void)
{
    int r = 1;
    goto done;
    r = 99;
done:
    r = r + 2;
    return r;
}
