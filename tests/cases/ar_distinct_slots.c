// expect: 1
/* Writing one element must not disturb its neighbours, which is what a wrong
   stride does. */
int main(void)
{
    int a[3];
    a[0] = 111111; a[1] = 222222; a[2] = 333333;
    return (a[0] == 111111) * (a[1] == 222222) * (a[2] == 333333);
}
