// expect: 5
/* An empty argument is still an argument. */
#define WRAP(x) (5 x)
int main(void)
{
    return WRAP();
}
