// expect: 7
/* Signedness decides the fold as it decides the instruction. "-1 < 1u" is 0 in
   C, because -1 becomes an enormous unsigned - so the label is 0 and it is the
   one taken. Folding it as signed would give 1 and fall to the default. */
int main(void)
{
    switch (0) {
    case (-1 < 1u): return 7;
    default: return 9;
    }
}
