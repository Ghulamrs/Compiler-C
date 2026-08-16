// expect: 0
// A file-scope initialiser that is an address rather than a number. C90 6.5.7
// allows one wherever a static initialiser is wanted, and it is a constant
// because the *linker* settles it - the address is unknown while compiling and
// still unknown after assembling, so what the compiler emits is a name and a
// byte offset, which is what a relocation carries.
//
// The functions are called add and sub on purpose. Both are x86 mnemonics, and
// MASM reserves every mnemonic as an identifier where C does not - so a table
// of pointers to them is also the case that catches a name reaching the data
// section without the mangling every other reference gets.
//
// Deliberately calls nothing from the C library, so it can run under every
// harness including tests/windows.sh - which executes Microsoft-convention
// code on Linux and works only for as long as nothing crosses into glibc.

struct P { int a; int b; char tag[4]; };

int value = 7;
int array[4] = { 10, 20, 30, 40 };
struct P rec = { 1, 2, "xy" };
static int hidden = 99;

int add(int x, int y) { return x + y; }
int sub(int x, int y) { return x - y; }

int   *p_value  = &value;               /* the plain case                */
int   *p_array  = array;                /* an array name decays          */
int   *p_elem   = &array[2];            /* a subscript, folded to +8     */
int   *p_member = &rec.b;               /* through a member, +4          */
char  *p_tag    = rec.tag;              /* an array *member* decays      */
int   *p_static = &hidden;              /* static duration, not external */
int   *p_offset = array + 3;            /* pointer plus a constant       */
char  *p_bytes  = (char *)&value + 1;   /* cast, then a byte offset      */
int  (*p_fn)(int, int) = add;           /* a function name is an address */
int  (*table[2])(int, int) = { add, sub };
int   *p_null   = 0;                    /* still an ordinary integer     */

int main(void)
{
    return (*p_value != 7)
         + (p_array[0] != 10)
         + (*p_elem != 30)
         + (*p_member != 2)
         + (p_tag[0] != 'x' || p_tag[1] != 'y' || p_tag[2] != 0)
         + (*p_static != 99)
         + (*p_offset != 40)
         + (*p_bytes != 0)          /* the second byte of 7, little-endian */
         + (p_fn(20, 22) != 42)
         + (table[0](20, 22) != 42)
         + (table[1](50, 8) != 42)
         + (p_null != 0);
}
