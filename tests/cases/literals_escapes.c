// expect: 0
// Two things that used to be quietly wrong rather than refused, which is the
// worst failure this compiler can have: it compiled, it ran, and it printed
// something else.
//
// The escapes were read one character at a time, so '\101' became the letter 0
// followed by stray digits and "\x41" became the letters x41. The largest
// literals went through strtol, which saturates, so ULONG_MAX arrived as
// LONG_MAX. gcc builds this file too, and every line below is a line where the
// two would have disagreed.
#include <stdio.h>

int main(void) {
    char buf[16];

    printf("named  : %d %d %d %d\n", '\n', '\t', '\r', '\\');
    printf("more   : %d %d %d %d\n", '\a', '\b', '\f', '\v');
    printf("quotes : %d %d %d\n", '\'', '\"', '\?');

    /* Octal: one to three digits, and \0 is just the shortest of them. */
    printf("octal  : %d %d %d %d\n", '\0', '\7', '\101', '\012');

    /* Hex: as many digits as follow. */
    printf("hex    : %d %d %d\n", '\x41', '\x7', '\x2A');

    /* A character constant has the value of a char, and char is signed here. */
    printf("signed : %d %d\n", '\xff', '\x80');

    /* The same escapes inside a string, where they are bytes rather than a
       value - "\x41" is one character and it is 'A'. */
    printf("string : [%s] [%s] [%s]\n", "\x41\x42", "\101\102", "a\tb");
    printf("length : %d %d\n", (int)sizeof "\x41\x42", (int)sizeof "\101");

    buf[0] = '\x41'; buf[1] = '\102'; buf[2] = 0;
    printf("built  : %s\n", buf);

    /* Literals at the edges of each width. */
    printf("ints   : %d %u\n", 2147483647, 4294967295u);
    printf("longs  : %ld %lu\n", 9223372036854775807L, 18446744073709551615UL);
    printf("bigger : %lu\n", 18446744073709551615UL / 2);
    printf("hexlit : %d %ld %lu\n", 0x7fffffff, 0x7fffffffffffffffL,
           0xffffffffffffffffUL);
    printf("octlit : %d %d\n", 0777, 010);
    return 0;
}
