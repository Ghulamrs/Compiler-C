// expect: 9
/* A call inside an argument: the commas belong to the inner call, not the
   outer one. */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
int main(void)
{
    return MAX(MAX(1, 9), MAX(2, 3));
}
