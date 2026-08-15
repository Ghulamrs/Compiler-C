; The fifth and sixth arguments, placed by hand - the MASM twin of
; msabi_stack_args.S, for the ml64 pipeline.
;
; Microsoft x64 has four register slots, and then the stack: argument n at
; rsp + 8*n at the point of the call, so the fifth sits just above the 32
; bytes of shadow at rsp+32 and the sixth at rsp+40. This caller loads the
; four registers and the two stack slots from the specification and calls
; probe6(1,2,3,4,5,6); the callee is cc1's, and it must read each argument
; from where Microsoft says it was put.

PUBLIC main
EXTERN probe6:PROC

.CODE
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48

    mov rcx, 1
    mov rdx, 2
    mov r8, 3
    mov r9, 4
    mov DWORD PTR [rsp+32], 5
    mov DWORD PTR [rsp+40], 6

    call probe6

    add rsp, 48
    mov rsp, rbp
    pop rbp
    ret
main ENDP

END
