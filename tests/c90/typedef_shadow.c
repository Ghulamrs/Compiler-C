/* C90 6.1.2.1: a typedef name declared inside a block is a new declaration in
   that block's scope, and hides an outer one for the rest of the block. cc1
   reports "'T' is typedefed twice" instead, because typedefs_ is one flat
   vector with a name->index map over it and no scopes - so the inner
   declaration is not a new binding, it is a collision.

   This is the remaining half of the block-scope typedef work. The other half
   is done: a typedef inside a block IS accepted now, and the hang that once
   made it be reverted is fixed. Only shadowing an outer name is left. */
typedef int T;
int main(void){ { typedef long T; T y = 2; return (int)y - 2; } }
