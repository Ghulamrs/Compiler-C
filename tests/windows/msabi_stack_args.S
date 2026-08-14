/* A hand-written Microsoft x64 caller that passes more arguments than there
 * are registers, which is the only way the shadow area becomes observable.
 *
 * It was not observable before, and that is why this file exists. Setting
 * shadowBytes to zero and rebuilding left the whole suite passing: cc1 was on
 * both ends of every call, so a caller that reserved nothing and a callee that
 * expected nothing agreed perfectly, and msabi_slots.S passes only four
 * arguments and never reads the stack at all. A rule that no case can break is
 * not being tested.
 *
 * The layout below is the specification's. At the point of the call:
 *
 *     %rcx %rdx %r8 %r9      arguments one to four
 *     0(%rsp) .. 31(%rsp)    the shadow area, the callee's to spill into
 *     32(%rsp)               argument five
 *     40(%rsp)               argument six
 *
 * So the callee sees argument five at 48(%rbp) - eight for the saved %rbp,
 * eight for the return address, thirty-two for the shadow. A backend that
 * forgot the shadow would look at 16(%rbp) and find the saved frame pointer.
 *
 * 48 bytes keeps %rsp on its sixteen-byte boundary, the push of %rbp having
 * squared it on entry.
 */
    .text
    .globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp

    mov $1, %rcx
    mov $2, %rdx
    mov $3, %r8
    mov $4, %r9
    movl $5, 32(%rsp)
    movl $6, 40(%rsp)

    call probe6

    add $48, %rsp
    mov %rbp, %rsp
    pop %rbp
    ret
