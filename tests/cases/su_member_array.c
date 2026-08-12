// expect: 1
/* 16 bytes of char plus a 4-byte int is 20, not 24: the struct aligns to 4,
   because char[16] aligns to 1 and the int is the widest member. */
struct Buf { char data[16]; int used; };
int main(void) { struct Buf b; b.data[0] = 65; b.used = 1;
                 return (b.data[0] == 65) * (sizeof b.data == 16) * (sizeof b == 20); }
