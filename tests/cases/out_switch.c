// expect: 0
int printf(char *fmt, ...);
int main(void)
{
    int i = 0;
    while (i < 7) {
        switch (i % 3) {
        case 0: printf("zero "); break;
        case 1: printf("one "); break;
        default: printf("many "); break;
        }
        i = i + 1;
    }
    printf("\n");
    return 0;
}
