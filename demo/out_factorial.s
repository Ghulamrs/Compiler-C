  .globl main
  .text
main:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  mov $5, %rax
  movl %eax, -4(%rbp)
  movslq %eax, %rax
  movslq -4(%rbp), %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call fact
  movslq %eax, %rax
  movl %eax, -8(%rbp)
  movslq %eax, %rax
  mov $100, %rax
  push %rax
  movslq -8(%rbp), %rax
  pop %rdi
  cdq
  idiv %edi
  movslq %eax, %rax
  push %rax
  mov $48, %rax
  pop %rdi
  add %edi, %eax
  movslq %eax, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  movslq %eax, %rax
  mov $10, %rax
  push %rax
  mov $10, %rax
  push %rax
  movslq -8(%rbp), %rax
  pop %rdi
  cdq
  idiv %edi
  movslq %eax, %rax
  pop %rdi
  cdq
  idiv %edi
  mov %edx, %eax
  movslq %eax, %rax
  push %rax
  mov $48, %rax
  pop %rdi
  add %edi, %eax
  movslq %eax, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  movslq %eax, %rax
  mov $10, %rax
  push %rax
  movslq -8(%rbp), %rax
  pop %rdi
  cdq
  idiv %edi
  mov %edx, %eax
  movslq %eax, %rax
  push %rax
  mov $48, %rax
  pop %rdi
  add %edi, %eax
  movslq %eax, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  movslq %eax, %rax
  mov $10, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  movslq %eax, %rax
  mov $0, %rax
  jmp .L.return.main
  mov $0, %rax
.L.return.main:
  mov %rbp, %rsp
  pop %rbp
  ret
  .globl fact
  .text
fact:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  mov %rdi, %rax
  movl %eax, -4(%rbp)
  mov $1, %rax
  push %rax
  movslq -4(%rbp), %rax
  pop %rdi
  cmp %edi, %eax
  setle %al
  movzbq %al, %rax
  cmp $0, %rax
  je .L.end.0
  mov $1, %rax
  jmp .L.return.fact
.L.end.0:
  mov $1, %rax
  push %rax
  movslq -4(%rbp), %rax
  pop %rdi
  sub %edi, %eax
  movslq %eax, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call fact
  movslq %eax, %rax
  push %rax
  movslq -4(%rbp), %rax
  pop %rdi
  imul %edi, %eax
  movslq %eax, %rax
  jmp .L.return.fact
  mov $0, %rax
.L.return.fact:
  mov %rbp, %rsp
  pop %rbp
  ret
