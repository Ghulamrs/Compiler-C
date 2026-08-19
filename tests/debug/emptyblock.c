// A block that declares something and emits no instructions.
//
// Nothing here generates code: a 'static' local is a global wearing a local's
// name, so its initialisation happens before the program runs. The block's two
// labels therefore land at the same address, and a lexical block whose low_pc
// equals its high_pc says the program counter is never inside it - so a
// debugger asked for the name answers that there is no such name, anywhere in
// the function.
//
// That is what clang does instead, and it is the answer this follows: no
// lexical block at all for a block that emits nothing, and the names it
// declared written one level up. There is no line inside such a block to stop
// on, which is the other half of the reason - a name only reachable from
// instructions that do not exist is not reachable at all.
//
// This case failed before the block was flattened, and passed before lexical
// blocks existed at all, which is the shape of a regression rather than a gap.
//
// stop: 29
// print: kept 5
// print: x 3

int f(void)
{
    int kept = 5;
    {
        static int x = 3;
    }
    return kept;
}

int main(void)
{
    return f();
}
