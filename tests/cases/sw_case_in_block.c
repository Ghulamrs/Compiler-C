// expect: 33
/* A case may sit inside a nested block. The jump lands in the middle of it,
   which is safe because the prologue allocated every slot already. */
int main(void)
{
    int r = 0;
    switch (2) {
    case 1:
        r = 11;
        break;
    {
    case 2:
        r = 33;
        break;
    }
    default:
        r = 99;
    }
    return r;
}
