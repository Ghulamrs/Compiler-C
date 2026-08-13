// expect: 0
/* The unit that borrows it, with nothing but declarations - which is exactly
   what a header would have given it. */
int printf(char *, ...);
extern int shared;
extern int table[];
int bump(int);
int hidden_probe(void);

int main(void)
{
    printf("shared=%d\n", shared);
    printf("after bump=%d\n", bump(1));
    shared = shared + 1;
    printf("written from here=%d\n", shared);
    table[0] = 7;
    table[3] = 9;
    printf("table=%d %d\n", table[0], table[3]);
    printf("its own static=%d\n", hidden_probe());
    return 0;
}
