// expect: 16
/* A length worked out from sizeof, which needs the target to answer before the
   array type can be built. */
int main(void)
{
    char buf[sizeof(int) * 4];
    return sizeof(buf);
}
