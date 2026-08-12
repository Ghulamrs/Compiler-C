  .globl main
  .text
main:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  lea -4(%rbp), %rax
  push %rax
  mov $48, %rax
  pop %rdi
  movl %eax, (%rdi)
  movslq %eax, %rax
  lea -8(%rbp), %rax
  push %rax
  mov $18, %rax
  pop %rdi
  movl %eax, (%rdi)
  movslq %eax, %rax
.L.main.begin.0:
  mov $0, %rax
  push %rax
  lea -8(%rbp), %rax
  movslq (%rax), %rax
  pop %rdi
  cmp %edi, %eax
  setne %al
  movzbq %al, %rax
  cmp $0, %rax
  je .L.main.end.0
  lea -12(%rbp), %rax
  push %rax
  lea -8(%rbp), %rax
  movslq (%rax), %rax
  push %rax
  lea -4(%rbp), %rax
  movslq (%rax), %rax
  pop %rdi
  cdq
  idiv %edi
  mov %edx, %eax
  movslq %eax, %rax
  pop %rdi
  movl %eax, (%rdi)
  movslq %eax, %rax
  lea -4(%rbp), %rax
  push %rax
  lea -8(%rbp), %rax
  movslq (%rax), %rax
  pop %rdi
  movl %eax, (%rdi)
  movslq %eax, %rax
  lea -8(%rbp), %rax
  push %rax
  lea -12(%rbp), %rax
  movslq (%rax), %rax
  pop %rdi
  movl %eax, (%rdi)
  movslq %eax, %rax
  jmp .L.main.begin.0
.L.main.end.0:
  lea -4(%rbp), %rax
  movslq (%rax), %rax
  jmp .L.return.main
  mov $0, %rax
.L.return.main:
  mov %rbp, %rsp
  pop %rbp
  ret
