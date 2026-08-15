// expect: 9
/* '(*f)(x)' is the older spelling of a call through a function pointer, and it
   was refused with a message about the return type - the deref produced a
   function type and nothing downstream could use one. C says a dereferenced
   function pointer is a function designator that converts straight back to a
   pointer, so '*' is the identity here and any number of them mean the same. */
int twice(int n) { return n * 2; }
int plus(int n)  { return n + 1; }

int main(void)
{
    int (*f)(int) = twice;
    int (*table[2])(int);

    table[0] = twice;
    table[1] = plus;

    return (*f)(3) + (**f)(0) + (*table[1])(2);
}
