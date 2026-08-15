/* A compound assignment whose target has a side effect in it.

   'a[i++] += 1' is valid C90: the target is evaluated once, i is incremented
   once, and the element that was read is the element written. cc1 rewrites
   'x op= e' as 'x = x op e', which needs a second copy of the target, so it
   can only accept targets that survive being evaluated twice - and 'i++' does
   not. It refuses by naming that rule rather than the parser's difficulty.

   This is the residue of a wider refusal. Until the clone reached through a
   subscript, every 'x[i] += ...' landed here too; now only the effectful ones
   do, which is a much smaller and much rarer program. */
int a[4];
int i;

int main(void)
{
    a[i++] += 1;
    return 0;
}
