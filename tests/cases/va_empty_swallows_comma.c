// expect: 0
/* With nothing to put after the format, ", ## __VA_ARGS__" deletes the comma
   before it - without which this expands to printf("...",) and will not
   compile. */
int printf(char *fmt, ...);
#define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
int main(void)
{
    LOG("none\n");
    LOG("one %d\n", 7);
    LOG("two %d %d\n", 8, 9);
    return 0;
}
