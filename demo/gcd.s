  .globl main
  .text
main:
  push %rbp
  mov %rsp, %rbp
  sub $32, %rsp
  mov $48, %rax
  mov %rax, -8(%rbp)
  mov $18, %rax
  mov %rax, -16(%rbp)
.L.begin.0:
  mov $0, %rax
  push %rax
  mov -16(%rbp), %rax
  pop %rdi
  cmp %rdi, %rax
  setne %al
  movzb %al, %rax
  cmp $0, %rax
  je .L.end.0
  mov -16(%rbp), %rax
  push %rax
  mov -8(%rbp), %rax
  pop %rdi
  cqo
  idiv %rdi
  mov %rdx, %rax
  mov %rax, -24(%rbp)
  mov -16(%rbp), %rax
  mov %rax, -8(%rbp)
  mov -24(%rbp), %rax
  mov %rax, -16(%rbp)
  jmp .L.begin.0
.L.end.0:
  mov -8(%rbp), %rax
  jmp .L.return
  mov $0, %rax
.L.return:
  mov %rbp, %rsp
  pop %rbp
  ret
