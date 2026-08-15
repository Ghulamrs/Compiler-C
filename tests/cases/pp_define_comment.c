// expect: 7
/* C removes comments in translation phase 3, before a directive is looked at in
   phase 4, so a macro body never contains one. cc1 kept them, and the damage
   hid: the comment travelled into the expansion, landed in the emitted text,
   and the lexer dropped it again - so every ordinary use worked. Only the
   conditional noticed, where the constant evaluator met the slash-star and
   reported a stray asterisk.

   Found by pointing cc1 at the macOS SDK, which defines _FORTIFY_SOURCE as the
   digit 2 followed by a comment, and then tests it in exactly such a
   conditional. */
#define LEVEL 2 /* on by default */
#define WIDTH /* leading */ 5
#define BOTH /* both */ 3 /* sides */

#if LEVEL > 0 && WIDTH > 4
int reached(void) { return LEVEL + WIDTH; }
#else
int reached(void) { return 0; }
#endif

int main(void)
{
    int a[BOTH];
    a[0] = reached();
    return a[0] - BOTH + 3;
}
