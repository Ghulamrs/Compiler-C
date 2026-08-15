/* GNU's rule, not C's: ', ## __VA_ARGS__' deletes the comma before it when the
   variable part is empty. Also a deliberate extension - without it the idiom
   that motivates variadic macros expands to a call with a trailing comma. */
#define LOG(fmt, ...) sink(fmt, ## __VA_ARGS__)
int sink(char *fmt) { return fmt[0] == 'x' ? 0 : 1; }
int main(void)
{
    return LOG("x");
}
