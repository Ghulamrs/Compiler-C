/* A caller written from the Microsoft x64 convention rather than from cc1.
 *
 * Everywhere else in this suite cc1 is on both ends of the call, so a rule it
 * has wrong it has wrong symmetrically and the answer still comes out right.
 * Here the caller is this file. If cc1 and the specification disagree about
 * where an argument travels, the disagreement is the failure.
 *
 * The rule being pinned is the positional one, and it is the difference that
 * separates this ABI from System V. Argument n takes slot n, in whichever
 * register file it belongs to, and spending a slot in one file spends it in
 * the other. So for probe(int, double, int, double):
 *
 *     a  slot 0  integer   %rcx        System V would say %rdi
 *     b  slot 1  floating  %xmm1       System V would say %xmm0
 *     c  slot 2  integer   %r8         System V would say %rsi
 *     d  slot 3  floating  %xmm3       System V would say %xmm1
 *
 * Not one of the four agrees, so a backend that quietly kept counting the two
 * files independently would fail on every argument rather than by luck.
 *
 * Alignment: %rsp is 8 past a 16-byte boundary on entry, the push of %rbp
 * squares it, and the 32 bytes of shadow keep it square - which is what
 * Microsoft x64 asks for at the point of the call.
 */
    .text
    .globl main
main:
    push %rbp
    mov %rsp, %rbp

    mov $7, %rcx
    movsd b_val(%rip), %xmm1
    mov $5, %r8
    movsd d_val(%rip), %xmm3

    sub $32, %rsp
    call probe
    add $32, %rsp

    mov %rbp, %rsp
    pop %rbp
    ret

    .section .rodata
    .align 8
b_val:  .double 2.0
d_val:  .double 4.0
