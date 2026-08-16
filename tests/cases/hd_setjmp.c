// expect: 0
// <setjmp.h>, and specifically the spelling that used to corrupt memory.
//
// 'r = setjmp(env)' is the idiom every program writes, and it was the one that
// did not work: the destination address of the assignment was computed before
// the call and parked below the stack pointer while setjmp ran. longjmp
// restores the stack pointer to what setjmp recorded, so on the second return
// those bytes had already been popped, freed and handed to some other call -
// and the store went through whatever now sat there.
//
// churn() is what makes this case an assertion rather than a hope. Between the
// two returns it fills the stack below main's frame with a known pattern, so a
// compiler holding anything down there reads the pattern back instead of its
// address. Without the fix in the two Assign visitors this case does not
// merely print the wrong number, it faults.
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static jmp_buf env_val;
static jmp_buf env_zero;
static jmp_buf env_cmp;

static int churned = 0;

static void churn(int n) {
    char pad[128];
    memset(pad, (char)(n & 0x7f), sizeof pad);
    churned = churned + (int)(unsigned char)pad[0];
    if (n > 0) churn(n - 1);
}

static void go_val(void)  { churn(6); longjmp(env_val, 42); }
static void go_zero(void) { churn(6); longjmp(env_zero, 0); }

int main(void) {
    volatile int witness = 1234;
    int r;

    // The assignment form, across six frames of scribbled-on stack.
    r = setjmp(env_val);
    if (r == 0) go_val();
    printf("val r=%d witness=%d\n", r, witness);

    // C90 7.6.2.1: longjmp with a zero value makes setjmp return 1, so that
    // the first return and a later one can never be confused.
    r = setjmp(env_zero);
    if (r == 0) go_zero();
    printf("zero r=%d\n", r);

    // The comparison form. This one always worked, because nothing is stored
    // and no address is held across the call - which is why shipping the
    // header before the fix would have meant shipping one working idiom and
    // one that ate the heap.
    if (setjmp(env_cmp) == 0) longjmp(env_cmp, 7);
    else                      printf("cmp ok\n");

    printf("churned=%d\n", churned);
    return (r == 1 && witness == 1234) ? 0 : 1;
}
