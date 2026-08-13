// expect: 9
#define N 4
#undef N
#define N 9
int main(void)
{
    return N;
}
