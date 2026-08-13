// expect: 3
/* A later initialiser sees the name declared before it in the same statement,
   which is what C requires and what makes the order of the Block matter. */
int main(void)
{
    int a = 1, b = a + 1;
    return a + b;
}
