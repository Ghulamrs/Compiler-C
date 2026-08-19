// The objects that are not simply a slot on the stack: a static local, which
// lives at an address and outlives its block; a pointer followed into another
// object; and a function pointer, whose type is the one place a missing
// parameter list would go unnoticed - 'int (*)()' is not a mistake in C90,
// it is a different type.
//
// stop: 30
// func: probe 25
// print: kept 42
// print: a.value 109
// print: a.next->value 20
// bt: probe main

struct Node { int value; struct Node *next; };

int twice(int n) { return n * 2; }

int probe(void)
{
    static int kept = 41;
    int (*fp)(int);
    struct Node a;
    struct Node b;

    a.value = 10; a.next = &b;
    b.value = 20; b.next = 0;
    fp = twice;
    kept = kept + 1;
    a.value = a.value + 99;
    return fp(a.value) + a.next->value + kept;
}

int main(void) { return probe(); }
