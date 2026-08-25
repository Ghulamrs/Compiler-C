/* A string literal whose whole content is one character.
 *
 * Token::is compared only the token's text, and a string's text is its
 * CONTENTS while a punctuator's text is its spelling - so the literal
 * "*" answered yes to is("*") and every parser test for an operator
 * matched it. printf("%s\n", "*") did not compile: at the start of an
 * expression the parser saw what it took to be a dereference and asked
 * for an operand.
 *
 * The set that failed was exactly what can begin a unary-expression -
 * the six unary operators and '('. The others here never failed, and
 * are in the case anyway: "/" and "%" survived only by being unable to
 * start an expression and falling through to the primary parser, which
 * does look at the kind. That is luck rather than correctness, and a
 * case that held only the seven would not say so.
 *
 * clang is the reference, so what this really asserts is that cc1 now
 * prints what clang prints.
 */

#include <stdio.h>

static void show(int n, const char *op)
{
    printf("%d %s\n", n, op);
}

int main(void)
{
    /* the seven that were refused */
    printf("%s %s %s %s %s %s %s\n", "*", "+", "-", "(", "&", "!", "~");

    /* a string argument after another argument, which is where this was
       first met - a trap helper taking a line and an operator */
    show(1, "*");
    show(2, "(");

    /* the ones that always worked, so the case describes the whole rule */
    printf("%s %s %s %s %s\n", "/", "%", "z", "ab", ")");

    /* and the same characters where they are genuinely operators, to be
       sure the fix did not cost the parser its actual punctuators */
    printf("%d %d %d\n", 6 * 7, +3, -4);
    printf("%d %d\n", (2 + 3) * 4, !0);

    return 0;
}
