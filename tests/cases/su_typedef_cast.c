// expect: 1
/* (Byte)x is a cast because Byte is a typedef name. Nothing but the symbol
   table can tell this from a multiplication. */
typedef unsigned char Byte;
int main(void) { int big = 300; return (Byte)big == 44; }
