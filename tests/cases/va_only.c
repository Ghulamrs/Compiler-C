// expect: 0
/* Nothing but '...': every argument is a variable one. */
int printf(char *fmt, ...);
#define OUT(...) printf(__VA_ARGS__)
int main(void)
{
    OUT("%s\n", "text");
    OUT("plain\n");
    return 0;
}
