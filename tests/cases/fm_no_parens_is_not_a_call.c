// expect: 4
/* A function-like macro used without parentheses is an ordinary identifier,
   which is what lets a variable share its name. */
#define TAKE(x) ((x) + 100)
int main(void)
{
    int TAKE = 4;
    return TAKE;
}
