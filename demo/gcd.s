  .globl main
  .text
main:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  mov $48, %rax
  movl %eax, -4(%rbp)
  movslq %eax, %rax
  mov $18, %rax
  movl %eax, -8(%rbp)
  movslq %eax, %rax
.L.begin.0:
  mov $0, %rax
  push %rax
  movslq -8(%rbp), %rax
  pop %rdi
  cmp %edi, %eax
  setne %al
  movzbq %al, %rax
  cmp $0, %rax
  je .L.end.0
  movslq -8(%rbp), %rax
  push %rax
  movslq -4(%rbp), %rax
  pop %rdi
  cdq
  idiv %edi
  mov %edx, %eax
  movslq %eax, %rax
  movl %eax, -12(%rbp)
  movslq %eax, %rax
  movslq -8(%rbp), %rax
  movl %eax, -4(%rbp)
  movslq %eax, %rax
  movslq -12(%rbp), %rax
  movl %eax, -8(%rbp)
  movslq %eax, %rax
  jmp .L.begin.0
.L.end.0:
  movslq -4(%rbp), %rax
  jmp .L.return.main
  mov $0, %rax
.L.return.main:
  mov %rbp, %rsp
  pop %rbp
  ret
