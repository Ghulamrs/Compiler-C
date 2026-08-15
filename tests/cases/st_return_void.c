// expect: 7
/* A bare 'return' is how a void function leaves early, and it did not parse at
   all until now - 'return' always read an expression, so a void function was
   writable only if it never returned before its closing brace. No case in the
   corpus had one, which is how it survived 394 of them. */
int total;

void add(int n)
{
    if (n < 0) return;
    total = total + n;
}

void nothing(void)
{
    return;
}

int main(void)
{
    total = 0;
    add(3);
    add(-5);
    add(4);
    nothing();
    return total;
}
