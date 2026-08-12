  .globl main
  .text
main:
  push %rbp
  mov %rsp, %rbp
  sub $16, %rsp
  mov $5, %rax
  mov %rax, -8(%rbp)
  mov -8(%rbp), %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call fact
  mov %rax, -16(%rbp)
  mov $100, %rax
  push %rax
  mov -16(%rbp), %rax
  pop %rdi
  cqo
  idiv %rdi
  push %rax
  mov $48, %rax
  pop %rdi
  add %rdi, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  mov $10, %rax
  push %rax
  mov $10, %rax
  push %rax
  mov -16(%rbp), %rax
  pop %rdi
  cqo
  idiv %rdi
  pop %rdi
  cqo
  idiv %rdi
  mov %rdx, %rax
  push %rax
  mov $48, %rax
  pop %rdi
  add %rdi, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  mov $10, %rax
  push %rax
  mov -16(%rbp), %rax
  pop %rdi
  cqo
  idiv %rdi
  mov %rdx, %rax
  push %rax
  mov $48, %rax
  pop %rdi
  add %rdi, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
  mov $10, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call putchar
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
  mov %rdi, -8(%rbp)
  mov $1, %rax
  push %rax
  mov -8(%rbp), %rax
  pop %rdi
  cmp %rdi, %rax
  setle %al
  movzb %al, %rax
  cmp $0, %rax
  je .L.end.0
  mov $1, %rax
  jmp .L.return.fact
.L.end.0:
  mov $1, %rax
  push %rax
  mov -8(%rbp), %rax
  pop %rdi
  sub %rdi, %rax
  push %rax
  pop %rdi
  mov $0, %rax
  call fact
  push %rax
  mov -8(%rbp), %rax
  pop %rdi
  imul %rdi, %rax
  jmp .L.return.fact
  mov $0, %rax
.L.return.fact:
  mov %rbp, %rsp
  pop %rbp
  ret
