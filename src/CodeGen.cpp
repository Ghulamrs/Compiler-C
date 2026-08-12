#include "CodeGen.h"

#include <cstdio>
#include <cstdlib>
#include <ostream>

// A stack machine, deliberately. Register allocation is a later problem and a
// separable one; pushing every intermediate is always correct and makes the
// output readable against the tree that produced it. The cost is poor code,
// which is the right thing to be bad at first.

// System V, in order. Windows uses a different four, which is why the target
// is a class and not a flag.
static const char *const kArgRegs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

void X86_64Linux::push() {
    out_ << "  push %rax\n";
    depth_++;
}

void X86_64Linux::pop(const char *reg) {
    out_ << "  pop " << reg << "\n";
    depth_--;
}

// ---- expressions: every one leaves its value in %rax ----

void X86_64Linux::visit(const Num &n) {
    out_ << "  mov $" << n.value() << ", %rax\n";
}

void X86_64Linux::visit(const Var &n) {
    out_ << "  mov -" << n.offset() << "(%rbp), %rax\n";
}

void X86_64Linux::visit(const Assign &n) {
    n.value().accept(*this);
    out_ << "  mov %rax, -" << n.offset() << "(%rbp)\n";
    // The value stays in %rax: assignment is an expression in C, and
    // "a = b = 0" needs the inner one to hand something back.
}

void X86_64Linux::visit(const Unary &n) {
    n.operand().accept(*this);
    if (n.op() == '-') out_ << "  neg %rax\n";
}

void X86_64Linux::visit(const Binary &n) {
    // Right first, then left. Either order works for + and *, but -, / and %
    // are not commutative, and popping the left side last is what puts it in
    // %rax where the instruction wants it.
    n.rhs().accept(*this);
    push();
    n.lhs().accept(*this);
    pop("%rdi");
    // %rax = lhs, %rdi = rhs

    switch (n.op()) {
    case BinOp::Add: out_ << "  add %rdi, %rax\n";  return;
    case BinOp::Sub: out_ << "  sub %rdi, %rax\n";  return;
    case BinOp::Mul: out_ << "  imul %rdi, %rax\n"; return;

    case BinOp::Div:
    case BinOp::Mod:
        // cqo sign-extends %rax into %rdx:%rax, which is the dividend idiv
        // reads. Without it a negative numerator divides against garbage.
        out_ << "  cqo\n";
        out_ << "  idiv %rdi\n";
        // idiv leaves the quotient in %rax and the remainder in %rdx.
        if (n.op() == BinOp::Mod) out_ << "  mov %rdx, %rax\n";
        return;

    default:
        break;
    }

    const char *set = nullptr;
    switch (n.op()) {
    case BinOp::Eq: set = "sete";  break;
    case BinOp::Ne: set = "setne"; break;
    case BinOp::Lt: set = "setl";  break;
    case BinOp::Le: set = "setle"; break;
    case BinOp::Gt: set = "setg";  break;
    case BinOp::Ge: set = "setge"; break;
    default:
        std::fprintf(stderr, "codegen: unhandled binary operator\n");
        std::exit(1);
    }
    out_ << "  cmp %rdi, %rax\n";
    out_ << "  " << set << " %al\n";
    // set__ writes one byte and leaves the other seven as they were, so the
    // result has to be widened or a stale high byte becomes part of the answer.
    out_ << "  movzb %al, %rax\n";
}

void X86_64Linux::visit(const Call &n) {
    // Each argument is evaluated and stacked, then the stack is drained into
    // the argument registers in reverse. Evaluating straight into the registers
    // would not survive the second argument: computing it can call something
    // else, and the first register would be gone by the time the call happened.
    for (const ExprPtr &arg : n.args()) {
        arg->accept(*this);
        push();
    }
    for (std::size_t i = n.args().size(); i-- > 0; )
        pop(kArgRegs[i]);

    // %rsp must be a multiple of 16 at the call instruction. It was on entry,
    // and each outstanding push has moved it by 8 - so an odd depth is exactly
    // when it needs correcting. Getting this wrong is invisible until libc
    // executes an aligned SSE move against the stack and dies inside printf.
    bool pad = (depth_ % 2) != 0;
    if (pad) out_ << "  sub $8, %rsp\n";

    // %al carries the number of vector registers used, which a variadic callee
    // reads. None are, and saying so costs one instruction.
    out_ << "  mov $0, %rax\n";
    out_ << "  call " << n.name() << "\n";

    if (pad) out_ << "  add $8, %rsp\n";
    // The return value is already in %rax, which is where every expression
    // leaves its value.
}

// ---- statements ----

void X86_64Linux::visit(const ExprStmt &n) {
    n.expr().accept(*this);   // value computed, then discarded
}

void X86_64Linux::visit(const Return &n) {
    n.value().accept(*this);
    out_ << "  jmp " << returnLabel_ << "\n";
}

void X86_64Linux::visit(const Block &n) {
    for (const StmtPtr &s : n.body()) s->accept(*this);
}

void X86_64Linux::visit(const If &n) {
    int id = nextLabel();
    n.cond().accept(*this);
    out_ << "  cmp $0, %rax\n";
    if (n.elseArm()) {
        out_ << "  je .L.else." << id << "\n";
        n.thenArm().accept(*this);
        out_ << "  jmp .L.end." << id << "\n";
        out_ << ".L.else." << id << ":\n";
        n.elseArm()->accept(*this);
    } else {
        out_ << "  je .L.end." << id << "\n";
        n.thenArm().accept(*this);
    }
    out_ << ".L.end." << id << ":\n";
}

void X86_64Linux::visit(const While &n) {
    int id = nextLabel();
    out_ << ".L.begin." << id << ":\n";
    n.cond().accept(*this);
    out_ << "  cmp $0, %rax\n";
    out_ << "  je .L.end." << id << "\n";
    n.body().accept(*this);
    out_ << "  jmp .L.begin." << id << "\n";
    out_ << ".L.end." << id << ":\n";
}

// ---- functions ----

void X86_64Linux::emit(const Function &fn) {
    depth_ = 0;
    returnLabel_ = ".L.return." + fn.name();

    out_ << "  .globl " << fn.name() << "\n";
    out_ << "  .text\n";
    out_ << fn.name() << ":\n";
    out_ << "  push %rbp\n";
    out_ << "  mov %rsp, %rbp\n";
    if (fn.frameSize() > 0)
        out_ << "  sub $" << fn.frameSize() << ", %rsp\n";

    // The parameters arrive in registers and are spilled into the slots the
    // parser gave them. They are the first locals declared, in order, so the
    // nth parameter is always at -8*(n+1) and nothing has to be looked up.
    for (int i = 0; i < fn.paramCount(); i++)
        out_ << "  mov " << kArgRegs[i] << ", -" << (i + 1) * 8 << "(%rbp)\n";

    fn.body().accept(*this);

    // Falling off the end returns 0, which is what C promises for main. A
    // return statement jumps over this, so it only applies when control
    // actually reaches the closing brace.
    out_ << "  mov $0, %rax\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov %rbp, %rsp\n";
    out_ << "  pop %rbp\n";
    out_ << "  ret\n";

    // A leaked push is a corrupted frame, and the symptom shows up in the
    // caller rather than here. Cheaper to assert than to debug.
    if (depth_ != 0) {
        std::fprintf(stderr, "codegen: stack depth %d at the end of %s\n",
                     depth_, fn.name().c_str());
        std::exit(1);
    }
}

void X86_64Linux::run(const Program &program) {
    for (const Function &fn : program) emit(fn);
}
