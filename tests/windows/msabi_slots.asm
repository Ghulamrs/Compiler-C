; A caller written from the Microsoft x64 convention rather than from cc1 -
; the MASM twin of msabi_slots.S, for the ml64 pipeline. The argument is the
; same one that file makes at length: everywhere else cc1 is on both ends of
; the call, so a rule it has wrong it has wrong symmetrically. Here the caller
; is this file, written from the specification, and any disagreement about
; where an argument travels is the failure.
;
; The rule being pinned is the positional one. Argument n takes slot n in
; whichever register file it belongs to, and spending a slot in one file
; spends it in the other. So for probe(int, double, int, double):
;
;     a  slot 0  integer   rcx        System V would say rdi
;     b  slot 1  floating  xmm1       System V would say xmm0
;     c  slot 2  integer   r8         System V would say rsi
;     d  slot 3  floating  xmm3       System V would say xmm1
;
; Not one of the four agrees, so a backend that quietly kept counting the two
; files independently fails on every argument rather than by luck.
;
; Alignment: rsp is 8 past a 16-byte boundary on entry, the push of rbp
; squares it, and the 32 bytes of shadow keep it square - which is what
; Microsoft x64 asks for at the point of the call.

PUBLIC main
EXTERN probe:PROC

.CONST
  ALIGN 8
b_val REAL8 2.0
d_val REAL8 4.0

.CODE
main PROC
    push rbp
    mov rbp, rsp

    mov rcx, 7
    movsd xmm1, REAL8 PTR [b_val]
    mov r8, 5
    movsd xmm3, REAL8 PTR [d_val]

    sub rsp, 32
    call probe
    add rsp, 32

    mov rsp, rbp
    pop rbp
    ret
main ENDP

END
