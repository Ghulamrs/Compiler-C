// expect: 91
// The callee half of the shadow-area check. msabi_stack_args.S is the caller,
// written from the specification, and it puts arguments five and six above the
// thirty-two bytes it left for this function to spill into.
//
// Each argument carries a different weight, so reading one from the wrong place
// changes the answer instead of cancelling against another. 1 + 4 + 9 + 16 + 25
// + 36.
int probe6(int a, int b, int c, int d, int e, int f)
{
    return a * 1 + b * 2 + c * 3 + d * 4 + e * 5 + f * 6;
}
