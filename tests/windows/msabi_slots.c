// expect: 18
// The callee half of the positional-slot check. msabi_slots.S beside this file
// is the caller, and it is hand-written from the Microsoft convention, so this
// is the one case in the suite where cc1 is not marking its own work.
//
// 7 + 2 + 5 + 4. Each argument is added in, so getting any single slot wrong
// changes the answer rather than cancelling out.
int probe(int a, double b, int c, double d)
{
    return a + (int)b + c + (int)d;
}
