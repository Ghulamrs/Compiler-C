// expect: 0
// A compound assignment whose target has an effect in it. 'a[i++] += 1' is
// valid C90 and means: work out the element once, increment i once, and write
// back the element that was read.
//
// 'x op= e' reads x and writes it, so the target is needed twice. Where it can
// be rebuilt from its own parts it is, and the code is what writing
// 'x = x op e' by hand would give. Where it cannot - because evaluating it does
// something, and no copy of 'i++' increments only once - the target's *address*
// is taken once into a hidden slot and both halves go through that:
//
//     (t = &target, *t = *t op e)
//
// Every check below is about how many times something happened, which is the
// only thing that distinguishes the two spellings.
static int calls = 0;
static int idx(void) { calls++; return 1; }

struct S { int v; };

int a[4];
struct S rec[3];

int main(void)
{
    int i;
    int bad = 0;

    // The classic. i moves exactly once, and the element read is the one
    // written - a second evaluation would increment twice and write a[1].
    a[0] = 10; a[1] = 20; i = 0;
    a[i++] += 5;
    bad += (a[0] != 15) + (a[1] != 20) + (i != 1);

    // A call in the subscript, which is the case that cannot be papered over:
    // calling twice would be visible even if the answer came out right.
    a[1] = 20; calls = 0;
    a[idx()] *= 2;
    bad += (a[1] != 40) + (calls != 1);

    // A member of an element whose index has an effect.
    rec[0].v = 7; i = 0;
    rec[i++].v += 3;
    bad += (rec[0].v != 10) + (i != 1);

    // Not only '+='. Each of these takes the same path.
    a[2] = 8;  i = 0; a[i++ + 2] -= 3;   bad += (a[2] != 5)  + (i != 1);
    a[2] = 8;  i = 0; a[i++ + 2] <<= 2;  bad += (a[2] != 32) + (i != 1);
    a[2] = 9;  i = 0; a[i++ + 2] %= 4;   bad += (a[2] != 1)  + (i != 1);
    a[2] = 12; i = 0; a[i++ + 2] /= 4;   bad += (a[2] != 3)  + (i != 1);
    a[2] = 12; i = 0; a[i++ + 2] &= 10;  bad += (a[2] != 8)  + (i != 1);

    // The whole expression has a value, and it is the value stored.
    //
    // Taken in its own statement on purpose. Putting the assignment and the
    // reads of a[3] and i as operands of one '+' would make this case depend
    // on the order those operands are evaluated in, which C leaves
    // unspecified - gcc reads a[3] before the assignment runs and cc1 reads it
    // after, and both are right. A test that cannot tell a wrong compiler from
    // a differently ordered one is not a test.
    a[3] = 1; i = 0;
    {
        int got = (a[i++ + 3] += 6);
        bad += (got != 7) + (a[3] != 7) + (i != 1);
    }

    // Prefix ++ and -- are compound assignments too, so they take the same
    // route when their target has an effect in it.
    a[0] = 5; i = 0; ++a[i++];  bad += (a[0] != 6) + (i != 1);
    a[0] = 5; i = 0; --a[i++];  bad += (a[0] != 4) + (i != 1);

    // And the ordinary pure targets still go the short way round.
    {
        int x = 4;
        int *p = &x;
        x += 3;    bad += (x != 7);
        *p *= 2;   bad += (x != 14);
        a[1] = 3;  a[1] += a[1];  bad += (a[1] != 6);
    }

    return bad;
}
