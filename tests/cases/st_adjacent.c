// expect: 0
// Adjacent string literals are one literal - C90 5.1.1.2, translation phase 6.
// It is the rule that lets a long format string be written down the page
// rather than off the right of it, which is why almost every C program with a
// printf longer than a line depends on it.
//
// The joining happens in the parser rather than the lexer, because macros are
// expanded before lexing: '"a" NAME "c"' arrives as three adjacent tokens and
// has to join like any other three. The macro cases below are the ones that
// would still fail if it were done a phase earlier.
#include <string.h>

#define NAME  "world"
#define EMPTY ""

int main(void)
{
    char *two    = "ab" "cd";
    char *four   = "one" "two" "three" "four";
    char *gapped = "a"  /* a comment between them changes nothing */  "b";

    // Down the page, which is the reason the rule exists.
    char *lines  = "the first part, "
                   "the second part, "
                   "and the third";

    char *macro  = "hello, " NAME "!";      // joined through a macro
    char *empty  = "x" EMPTY "y";           // an empty literal in the run
    char *esc    = "tab\there" "\tand more";

    // As an array initialiser, where the joined length decides the size.
    char arr[] = "arr" "ay";

    // A '\0' inside a literal is a byte like any other, and joining does not
    // stop at it: this is 'a', NUL, 'b', 'c', and then the terminator.
    char nul[] = "a\0b" "c";

    // Commas keep literals apart - these must NOT join.
    char *list[] = { "x", "y", "z" };

    return (strcmp(two,    "abcd")               != 0)
         + (strcmp(four,   "onetwothreefour")    != 0)
         + (strcmp(gapped, "ab")                 != 0)
         + (strcmp(lines,  "the first part, the second part, and the third") != 0)
         + (strcmp(macro,  "hello, world!")      != 0)
         + (strcmp(empty,  "xy")                 != 0)
         + (strcmp(esc,    "tab\there\tand more") != 0)
         + (strcmp(arr,    "array")              != 0)
         + ((int)sizeof arr != 6)
         + ((int)sizeof nul != 5)
         + (nul[1] != 0 || nul[2] != 'b' || nul[3] != 'c')
         + (strcmp(list[0], "x") != 0)
         + (strcmp(list[2], "z") != 0)
         + ((int)(sizeof list / sizeof list[0]) != 3);
}
