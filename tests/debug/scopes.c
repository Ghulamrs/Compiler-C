// A name declared more than once in one function, asked for from inside the
// block that redeclares it.
//
// This is the check lexical blocks exist for. Without them every declaration
// a function makes sits in one flat list under the subprogram, and a debugger
// asked for a shadowed name answers with whichever it met first - which is
// the outer one, everywhere, including here where it is not in scope. The
// name being known is not the question; which binding is, is.
//
// stop: 26
// print: n 20
// print: inner 7
// print: total 14
// func: shadowed 18

int shadowed(void)
{
    int n = 3;
    int total = 0;
    {
        int n = 20;
        int inner = 7;
        for (int i = 0; i < 4; i++) {
            total += i * i;
        }
        total = total + n + inner;
    }
    return total + n;
}

int main(void)
{
    return shadowed();
}
