/* C99. Recorded here rather than only in prose: docs/STATUS.md names this as a
   deliberate extension, and this is where that claim can be checked. */
#define LOG(fmt, ...) sink(fmt, __VA_ARGS__)
int sink(char *fmt, int a) { return fmt[0] == 'x' ? a : 0; }
int main(void)
{
    return LOG("x", 0);
}
