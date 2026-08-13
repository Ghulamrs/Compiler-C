// expect: 9
/* A qualifier may sit either side of the type, and beside a storage class. */
const int a = 4;
int const b = 5;
int main(void)
{
    static const int c = 0;
    volatile const int d = 0;
    return a + b + c + d;
}
