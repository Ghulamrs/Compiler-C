// expect: 0
// '#line' - C90 6.8.4. It renames and renumbers the lines that follow, and
// changes nothing else: __LINE__, __FILE__ and every diagnostic report what it
// says, and the generated code is untouched.
//
// The reason it exists is a program that writes C. A parser generator emitting
// a .c from a .y wants an error in the output to point at the .y a person
// actually wrote, and '#line' is how it says so.
//
// The number applies to the *next* line, which is the part worth testing
// rather than assuming - and the third form, '#line pp-tokens', is expanded
// first, so a macro may supply either half.
#include <string.h>

#define WHERE 500
#define WHAT  "from-a-macro.y"

static int line_after_plain(void)
{
#line 100
    return __LINE__;            /* the line after the directive is 100 */
}

static int line_two_later(void)
{
#line 200
    ;
    return __LINE__;            /* 200 was the ';', so this is 201 */
}

static const char *renamed_file(void)
{
#line 300 "generated.y"
    return __FILE__;
}

static int macro_number(void)
{
#line WHERE WHAT
    return __LINE__;
}

static const char *macro_file(void)
{
#line WHERE WHAT
    return __FILE__;
}

// Each function above renumbers only itself: the directive's effect stops at
// the end of the file it is in, and this one carries on being what it is.
int main(void)
{
    return (line_after_plain() != 100)
         + (line_two_later()   != 201)
         + (strcmp(renamed_file(), "generated.y")  != 0)
         + (macro_number()     != 500)
         + (strcmp(macro_file(), "from-a-macro.y") != 0);
}
