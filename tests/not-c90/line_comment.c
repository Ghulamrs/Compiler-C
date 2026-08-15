/* '//' is C99. C90 has one comment form, and where the two disagree they
   disagree silently: in C90 the sequence 'a //b' divides a by the result of
   dividing by b, and here the rest of the line is a comment instead. */
int main(void)
{
    // not a C90 comment
    return 0;
}
