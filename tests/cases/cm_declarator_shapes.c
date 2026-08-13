// expect: 14
/* The '*' and the '[4]' belong to one name apiece; only the specifiers are
   shared. */
int main(void)
{
    int x = 4, *p = &x, a[4];
    a[0] = 10;
    return a[0] + *p;
}
