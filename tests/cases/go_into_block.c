// expect: 7
/* Jumping into a block. Legal C, and safe here because the prologue allocated
   every slot before any of this ran. */
int main(void)
{
    int r = 0;
    goto inner;
    {
        r = 99;
inner:
        r = r + 7;
    }
    return r;
}
