// expect: 0
/* Both arms are brought to one type, so this is a double even though the arm
   taken is the int one. */
int printf(char *fmt, ...);
int main(void)
{
    int n = 1;
    printf("%.2f\n", n ? 1 : 2.5);
    printf("%.2f\n", n ? 2.5 : 1);
    return 0;
}
