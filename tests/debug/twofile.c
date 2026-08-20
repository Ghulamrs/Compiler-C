// Two files in one program, and the breakpoint in the one that has no main.
//
// This is the case that was missing, and its absence hid a real bug for as
// long as the suite existed: every unit cc1 emitted said DW_AT_stmt_list is
// zero. Within one object that is true - the unit's line program does begin
// its line section. After the linker has laid three objects' line sections
// one after another it is false for all but the first, so gdb read the second
// file's lines out of the first file's table and answered "Line number 11 is
// out of range" for a file whose table was right there.
//
// One source per case cannot see that. Two can.
//
// with: twofile_other.c
// stopin: twofile_other.c 7

int twice(int n);

int main(void)
{
    int total = twice(21);
    return total == 42 ? 0 : 1;
}
