// expect: 0
// One object described by two declarations that do not say the same thing.
// C90 6.1.2.6 gives it the composite of the two, and for an array that is the
// one that knows its length - so neither order is an error and the bound
// belongs to whichever declaration states it.
//
// Every term below is zero when the composite is right, so a length that fails
// to arrive changes the answer rather than cancelling out. The sizeof terms
// check that the *type* was completed, and the subscripts that the object was
// laid out at that size.
extern int a[];
int a[3] = { 1, 2, 3 };          /* the extern first, then the definition */

int b[4];                        /* the definition first, then the extern  */
extern int b[];

extern int c[][3];               /* the length arrives one dimension down  */
int c[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };

int main(void)
{
    b[3] = 9;
    return (int)(sizeof(a) - 12) + (int)(sizeof(b) - 16)
         + (int)(sizeof(c) - 24)
         + (a[2] - 3) + (b[3] - 9) + (c[1][2] - 6)
         + b[0];                 /* b has no initialiser, so this is zero  */
}
