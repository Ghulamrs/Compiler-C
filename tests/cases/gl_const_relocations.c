// expect: 0
// A const object whose initialiser holds an address. Ordinary C, and it works
// here on every target - it is kept in the corpus so that the section it lands
// in cannot be changed for one target without the other two answering for it.
// On arm64-darwin it did not link at all until 2026-08-22: __TEXT,__const
// cannot hold the address of another __TEXT symbol, and the whole of the
// symptom was a linker refusing the program.
//
// Four shapes, because they reach the same rule by different roads: a static
// local, a file-scope pointer, an array of them, and a struct holding one.
#include <stdio.h>

static const char *const name = "file scope";
const char *const table[3] = { "one", "two", "three" };

struct Named { int n; const char *label; };
static const struct Named tagged = { 7, "in a struct" };

static const char *pick(int i)
{
    static const char *const inside = "static local";
    return i == 0 ? inside : table[i - 1];
}

int main(void)
{
    int bad = 0;

    if (name[0] != 'f') bad = bad + 1;
    if (pick(0)[0] != 's') bad = bad + 2;
    if (pick(2)[0] != 't') bad = bad + 4;
    if (table[2][4] != 'e') bad = bad + 8;
    if (tagged.n != 7 || tagged.label[0] != 'i') bad = bad + 16;

    printf("%s, %s, %s, %s\n", name, pick(0), table[0], tagged.label);
    return bad;
}
