// expect: 0
/* C90 6.8.2 gives '#include' a third form: pp-tokens that match neither the
   quoted nor the angled spelling are macro-expanded and read again. It is how a
   header name is kept in one place and included from several, and cc1 refused
   it as malformed rather than expanding it. */
#define WHICH <stdio.h>
#include WHICH

int main(void)
{
    printf("include-by-macro\n");
    return 0;
}
