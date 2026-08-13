// expect: 1
/* A char switch compares in int, and the case values are character constants. */
int main(void)
{
    char c = 'b';
    switch (c) {
    case 'a': return 0;
    case 'b': return 1;
    case 'c': return 2;
    }
    return 9;
}
