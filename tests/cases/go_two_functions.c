// expect: 3
/* The same label name in two functions. Function scope means these are two
   labels, and the assembler must not see one symbol twice. */
int f(void)
{
    int r = 1;
    goto done;
    r = 50;
done:
    return r;
}
int main(void)
{
    int r = 2;
    goto done;
    r = 50;
done:
    return r + f();
}
