// expect: 10
/* One declaration, several names, each with its own declarator. */
int main(void)
{
    int x, y = 2, z = 3;
    x = 5;
    return x + y + z;
}
