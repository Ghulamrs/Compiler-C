// expect: 0
// A function that returns a function pointer, which C spells by wrapping the
// name: in 'int (*get(void))(void)' the inner '(void)' is get's own parameter
// list and the outer one belongs to what get returns. Reading it outwards from
// the name: get is a function, taking void, returning a pointer to a function,
// taking void, returning int.
//
// The declarator is the whole difficulty. Everything after it is ordinary - a
// returned function pointer is an address like any other, and calling through
// it is the same call this compiler already made.
static int three(void) { return 3; }
static int four(void)  { return 4; }

// The prototype and the definition, which must agree.
static int (*pick(int which))(void);

static int (*pick(int which))(void)
{
    return which ? four : three;
}

// The same shape with parameters of its own, and one returning a pointer to a
// function that itself takes arguments.
static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }

static int (*op(char c))(int, int)
{
    return c == '+' ? add : sub;
}

// A returned pointer stored, and one called immediately where it stands.
int main(void)
{
    int (*f)(void) = pick(1);

    return (pick(0)() - 3)
         + (pick(1)() - 4)
         + (f() - 4)
         + (op('+')(20, 22) - 42)
         + (op('-')(50, 8) - 42);
}
