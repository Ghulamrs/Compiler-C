// expect: 0
// Everything the compare-and-branch chain has to get right: fallthrough, a
// default that is not last, a subject wider than a 12-bit immediate, negative
// and unsigned case values, a switch inside a switch, and 'break' and
// 'continue' inside a loop meaning two different things in the same body.
int printf(const char *fmt, ...);

int calls = 0;
int subject(void) { calls = calls + 1; return 2; }

int main(void) {
    int i;
    int r;
    int s;
    char c;
    long w;
    unsigned u;
    int bad;

    bad = 0;

    /* the plain shape, over every arm */
    for (i = 0; i < 5; i++) {
        switch (i) {
        case 0:  r = 100; break;
        case 1:
        case 2:  r = 200; break;
        default: r = 300; break;
        }
        if (i == 0 && r != 100) bad = bad + 1;
        if ((i == 1 || i == 2) && r != 200) bad = bad + 2;
        if (i >= 3 && r != 300) bad = bad + 4;
    }

    /* default first, and a case falling into the one below it */
    s = 0;
    for (i = 0; i < 4; i++) {
        switch (i) {
        default: s = s + 1000; break;
        case 1:  s = s + 10;
        case 2:  s = s + 100; break;
        }
    }
    if (s != 2210) bad = bad + 8;

    /* no arm matches and there is no default: the body must be skipped */
    r = 7;
    switch (99) {
    case 1: r = 1; break;
    case 2: r = 2; break;
    }
    if (r != 7) bad = bad + 16;

    /* values a 12-bit immediate cannot hold, and negative ones */
    w = 5000000000L;
    r = 0;
    switch (w) {
    case 1:           r = 1; break;
    case 5000000000L: r = 9; break;
    default:          r = -1;
    }
    if (r != 9) bad = bad + 32;

    i = -3;
    r = 0;
    switch (i) {
    case -3: r = 33; break;
    default: r = -1;
    }
    if (r != 33) bad = bad + 64;

    u = 4294967295u;
    r = 0;
    switch (u) {
    case 0:           r = 0; break;
    case 4294967295u: r = 5; break;
    default:          r = -1;
    }
    if (r != 5) bad = bad + 128;

    /* a char subject, which reaches the compare sign-extended */
    c = 'b';
    r = 0;
    switch (c) {
    case 'a': r = 1; break;
    case 'b': r = 2; break;
    }
    if (r != 2) bad = bad + 256;

    /* the subject is evaluated once, whichever arm is taken */
    r = 0;
    switch (subject()) {
    case 1:  r = 1; break;
    case 2:  r = 22; break;
    default: r = 0;
    }
    if (r != 22 || calls != 1) bad = bad + 512;

    /* nested, so the inner 'break' leaves only the inner switch */
    r = 0;
    switch (1) {
    case 1:
        switch (2) {
        case 2:  r = 12; break;
        default: r = 10;
        }
        r = r + 1;
        break;
    default:
        r = -1;
    }
    if (r != 13) bad = bad + 1024;

    /* 'break' ends the switch and 'continue' skips to the loop's next turn,
       from inside the same body */
    s = 0;
    for (i = 0; i < 6; i++) {
        switch (i) {
        case 0:  continue;
        case 3:  break;
        case 4:  s = s + 100; continue;
        default: s = s + 1;
        }
        s = s + 10;
    }
    if (s != 143) bad = bad + 2048;

    /* case labels inside a nested statement - the body is one statement, and
       a label in it is reachable from the head of the switch */
    {
        int count;
        int n;
        count = 0;
        i = 0;
        n = 7;
        switch (n % 4) {
        case 0: do { count++;
        case 3:      count++;
        case 2:      count++;
        case 1:      count++; i++;
                } while (i < 2);
        }
        if (count != 7) bad = bad + 4096;
    }

    if (bad != 0) printf("switch: %d\n", bad);
    return bad;
}
