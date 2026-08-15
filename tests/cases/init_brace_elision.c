// expect: 1
// C90 6.5.7: the braces round a subaggregate may be left out, and then the
// initialisers are taken from the list already being read until it is full.
// Every shape that rule has, at block scope and at file scope, because the two
// are walked by different code - one emits stores, the other lays out bytes.
struct P { int a[2]; int n; };
struct Q { struct P p; int z; };

int g2[2][3]      = {1,2,3,4,5,6};
int gpart[2][2]   = {{1,2},3,4};
int gshort[3][2]  = {1,2,3};
struct P gs       = {1,2,3};
struct Q gq       = {1,2,3,4};
int ginf[][3]     = {1,2,3,4,5,6};
char gstr[2][6]   = {"ab","cd"};

int main(void)
{
    int a[2][3]      = {1,2,3,4,5,6};      /* flat, two rows of three   */
    int b[2][2]      = {{1,2},3,4};        /* one row braced, one not   */
    int c[2][2][2]   = {1,2,3,4,5,6,7,8};  /* three deep                */
    int d[3][2]      = {1,2,3};            /* short: the rest is zero   */
    struct P s       = {1,2,3};            /* into a struct's array     */
    struct Q q       = {1,2,3,4};          /* into a nested struct      */
    struct P sa[2]   = {1,2,3,4,5,6};      /* into an array of structs  */
    int inf[][3]     = {1,2,3,4,5,6,7};    /* the length is rows, not 7 */
    char cs[2][6]    = {"ab","cd"};        /* a string per element      */
    int t1,t2,t3,t4,t5,t6,t7,t8,t9,tg;

    t1 = a[0][0]==1 && a[0][2]==3 && a[1][0]==4 && a[1][2]==6;
    t2 = b[0][0]==1 && b[0][1]==2 && b[1][0]==3 && b[1][1]==4;
    t3 = c[0][0][0]==1 && c[0][1][1]==4 && c[1][0][0]==5 && c[1][1][1]==8;
    t4 = d[0][0]==1 && d[0][1]==2 && d[1][0]==3 && d[1][1]==0 && d[2][0]==0;
    t5 = s.a[0]==1 && s.a[1]==2 && s.n==3;
    t6 = q.p.a[0]==1 && q.p.a[1]==2 && q.p.n==3 && q.z==4;
    t7 = sa[0].a[0]==1 && sa[0].n==3 && sa[1].a[0]==4 && sa[1].n==6;
    /* seven items, three per row, so three rows - and the tail is zero */
    t8 = (int)(sizeof inf / sizeof inf[0])==3 && inf[2][0]==7 && inf[2][1]==0;
    t9 = cs[0][0]=='a' && cs[0][2]==0 && cs[1][1]=='d';

    tg = g2[1][2]==6 && gpart[1][1]==4 && gshort[1][1]==0 && gshort[2][0]==0
      && gs.n==3 && gq.z==4 && gstr[1][0]=='c'
      && (int)(sizeof ginf / sizeof ginf[0])==2 && ginf[1][2]==6;

    return t1 && t2 && t3 && t4 && t5 && t6 && t7 && t8 && t9 && tg;
}
