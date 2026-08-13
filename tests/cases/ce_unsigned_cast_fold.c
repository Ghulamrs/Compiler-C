// expect: 5
/* The mirror of it: (unsigned int)-1 is 4294967295, which is greater than 100.
   Folded as signed it would be -1, and the default would be taken. */
int main(void)
{
    switch (1) {
    case ((unsigned int)-1 > 100u): return 5;
    default: return 9;
    }
}
