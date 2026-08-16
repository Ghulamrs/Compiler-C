// expect: 42
// One object in each of the four segments, and the two cases that decide
// which. A const object is read-only whatever its value, so ro_zero belongs
// with the constants and not with the zeroes; an ordinary object initialised
// to zero is indistinguishable from one left uninitialised, and both belong
// where the loader makes the zeroes rather than the file carrying them.
//
// Every object is added into the answer, so an object that lands in the wrong
// segment - or starts as something other than zero when it should be zero -
// changes the result rather than cancelling out.
const int  ro_five = 5;
const int  ro_zero = 0;
int        rw_seven = 7;
int        rw_zero = 0;
int        rw_none;
static int hidden = 3;
char       big[4096];

int main(void)
{
    big[4095] = 27;
    return ro_five + ro_zero + rw_seven + rw_zero + rw_none + hidden
         + big[4095];
}
