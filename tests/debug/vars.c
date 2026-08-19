// An object of every shape this compiler can describe, asked for by name.
//
// The composite ones are checked through a scalar expression - p.y rather
// than p - for two reasons: gdb and lldb print a struct differently while
// agreeing about what is in it, and a member offset that is wrong shows up
// in the member rather than in the whole. Every expected value below is
// distinct, so one debugger run answers all of them at once.
//
// stop: 38
// func: addUp 27
// print: total 107
// print: p.y 42
// print: nums[3] 9
// print: counter 7
// print: letter+0 81
// print: scale*2 5
// print: *ptr+1 108
// bt: addUp main

struct Point { int x; int y; };

int counter = 7;

int addUp(int n, double scale)
{
    int i;
    int total = 0;
    char letter = 'Q';
    struct Point p;
    int nums[4];
    int *ptr;

    p.x = 3;
    p.y = 42;
    for (i = 0; i < 4; i++) nums[i] = i * i;
    ptr = &total;
    total = n + p.x + nums[3] + letter + (int)scale + counter;
    return *ptr;
}

int main(void)
{
    return addUp(5, 2.5);
}
