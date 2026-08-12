#include "CodeGen.h"

#include <cstdio>
#include <cstdlib>
#include <ostream>

static const char *const kArgRegs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };

void X86_64Linux::push() { out_ << "  push %rax\n"; depth_++; }
void X86_64Linux::pop(const char *reg) { out_ << "  pop " << reg << "\n"; depth_--; }

// Arithmetic runs at the width of its type. Anything narrower than int has
// already been promoted by the parser, so only 4 and 8 reach here.
const char *X86_64Linux::acc(const Type *t) const {
    return t->size(target_) == 8 ? "%rax" : "%eax";
}
const char *X86_64Linux::rhs(const Type *t) const {
    return t->size(target_) == 8 ? "%rdi" : "%edi";
}

// The extension that puts %rax back in canonical form for t. This is where
// signedness stops being bookkeeping: the same eight bits become 255 or -1
// depending on which instruction is chosen.
void X86_64Linux::canonicalise(const Type *t) {
    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1) out_ << (sign ? "  movsbq %al, %rax\n" : "  movzbq %al, %rax\n");
    else if (sz == 2) out_ << (sign ? "  movswq %ax, %rax\n" : "  movzwq %ax, %rax\n");
    else if (sz == 4) out_ << (sign ? "  movslq %eax, %rax\n" : "  mov %eax, %eax\n");
    // 8 bytes is already the register; nothing to do.
}

void X86_64Linux::load(const Type *t, int offset) {
    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  movsbq -" : "  movzbq -") << offset << "(%rbp), %rax\n";
    else if (sz == 2) out_ << (sign ? "  movswq -" : "  movzwq -") << offset << "(%rbp), %rax\n";
    else if (sz == 4) out_ << (sign ? "  movslq -" : "  movl -") << offset
                           << (sign ? "(%rbp), %rax\n" : "(%rbp), %eax\n");
    else              out_ << "  movq -" << offset << "(%rbp), %rax\n";
}

void X86_64Linux::store(const Type *t, int offset) {
    switch (t->size(target_)) {
    case 1: out_ << "  movb %al, -"  << offset << "(%rbp)\n"; return;
    case 2: out_ << "  movw %ax, -"  << offset << "(%rbp)\n"; return;
    case 4: out_ << "  movl %eax, -" << offset << "(%rbp)\n"; return;
    default: out_ << "  movq %rax, -" << offset << "(%rbp)\n"; return;
    }
}

// ---- expressions ----

void X86_64Linux::visit(const Num &n) {
    out_ << "  mov $" << n.value() << ", %rax\n";
}

void X86_64Linux::visit(const Var &n) { load(n.type(), n.offset()); }

void X86_64Linux::visit(const Assign &n) {
    n.value().accept(*this);
    store(n.type(), n.offset());
    // The stored value is the value of the expression, and it must read back
    // as the narrower type would: char c; (c = 300) is 44, not 300.
    canonicalise(n.type());
}

void X86_64Linux::visit(const Unary &n) {
    n.operand().accept(*this);
    if (n.op() == '-') {
        out_ << "  neg " << acc(n.type()) << "\n";
        canonicalise(n.type());
    } else if (n.op() == '!') {
        out_ << "  cmp $0, %rax\n";
        out_ << "  sete %al\n";
        out_ << "  movzbq %al, %rax\n";
    }
}

// Narrowing and widening, and nothing else. The parser decided that this
// conversion happens; this only performs it.
void X86_64Linux::visit(const Cast &n) {
    n.value().accept(*this);
    if (!n.type()->isVoid()) canonicalise(n.type());
}

void X86_64Linux::visit(const Binary &n) {
    // Short circuit, and therefore branches rather than the push/pop pattern:
    // the right side must not be evaluated when the left has already decided
    // the answer. "0 && putchar(65)" prints nothing, and that is observable.
    if (n.op() == BinOp::LAnd || n.op() == BinOp::LOr) {
        int id = nextLabel();
        bool isAnd = n.op() == BinOp::LAnd;
        const char *shortJump = isAnd ? "je" : "jne";

        n.lhs().accept(*this);
        out_ << "  cmp $0, %rax\n";
        out_ << "  " << shortJump << " .L.sc." << id << "\n";
        n.rhs().accept(*this);
        out_ << "  cmp $0, %rax\n";
        out_ << "  " << shortJump << " .L.sc." << id << "\n";
        out_ << "  mov $" << (isAnd ? 1 : 0) << ", %rax\n";
        out_ << "  jmp .L.scend." << id << "\n";
        out_ << ".L.sc." << id << ":\n";
        out_ << "  mov $" << (isAnd ? 0 : 1) << ", %rax\n";
        out_ << ".L.scend." << id << ":\n";
        return;
    }

    n.rhs().accept(*this);
    push();
    n.lhs().accept(*this);
    pop("%rdi");

    // Both operands share a type by now - the parser converted them - so
    // either side answers the width and signedness question.
    const Type *t = n.lhs().type();
    const char *a = acc(t);
    const char *d = rhs(t);
    bool sign = t->isSigned(target_);
    bool wide = t->size(target_) == 8;

    switch (n.op()) {
    case BinOp::Add: out_ << "  add "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::Sub: out_ << "  sub "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::Mul: out_ << "  imul " << d << ", " << a << "\n"; canonicalise(n.type()); return;

    case BinOp::Div:
    case BinOp::Mod:
        // Signedness picks the instruction, not just the type. idiv sign-extends
        // the dividend into %rdx first; div must see a zeroed %rdx instead.
        if (sign) out_ << (wide ? "  cqo\n" : "  cdq\n");
        else      out_ << "  xor %edx, %edx\n";
        out_ << (sign ? "  idiv " : "  div ") << d << "\n";
        if (n.op() == BinOp::Mod) out_ << (wide ? "  mov %rdx, %rax\n" : "  mov %edx, %eax\n");
        canonicalise(n.type());
        return;

    case BinOp::Shl:
    case BinOp::Shr:
        // The count belongs in %cl. Arithmetic shift for signed, logical for
        // unsigned: -1 >> 1 is -1, but (unsigned)-1 >> 1 is 2147483647.
        out_ << "  mov %rdi, %rcx\n";
        if (n.op() == BinOp::Shl) out_ << "  shl %cl, " << a << "\n";
        else                      out_ << (sign ? "  sar %cl, " : "  shr %cl, ") << a << "\n";
        canonicalise(n.type());
        return;

    default:
        break;
    }

    const char *set = nullptr;
    switch (n.op()) {
    case BinOp::Eq: set = "sete";  break;
    case BinOp::Ne: set = "setne"; break;
    // Signed and unsigned comparison are different instructions. Using the
    // signed form on unsigned operands is how -1 < 1u comes out true.
    case BinOp::Lt: set = sign ? "setl"  : "setb"; break;
    case BinOp::Le: set = sign ? "setle" : "setbe"; break;
    case BinOp::Gt: set = sign ? "setg"  : "seta"; break;
    case BinOp::Ge: set = sign ? "setge" : "setae"; break;
    default:
        std::fprintf(stderr, "codegen: unhandled binary operator\n");
        std::exit(1);
    }
    out_ << "  cmp " << d << ", " << a << "\n";
    out_ << "  " << set << " %al\n";
    out_ << "  movzbq %al, %rax\n";
}

void X86_64Linux::visit(const Call &n) {
    for (const ExprPtr &arg : n.args()) {
        arg->accept(*this);
        push();
    }
    for (std::size_t i = n.args().size(); i-- > 0; )
        pop(kArgRegs[i]);

    bool pad = (depth_ % 2) != 0;
    if (pad) out_ << "  sub $8, %rsp\n";
    out_ << "  mov $0, %rax\n";
    out_ << "  call " << n.name() << "\n";
    if (pad) out_ << "  add $8, %rsp\n";

    // A callee returns in %eax for an int-sized result and leaves the high
    // half undefined, so the value has to be put back into canonical form.
    if (!n.type()->isVoid()) canonicalise(n.type());
}

// ---- statements ----

void X86_64Linux::visit(const ExprStmt &n) { n.expr().accept(*this); }

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
    if (fn.frameSize() > 0) out_ << "  sub $" << fn.frameSize() << ", %rsp\n";

    // Each argument register is stored with the width of its own parameter: a
    // char parameter occupies one byte of the frame, not eight.
    const std::vector<Param> &ps = fn.params();
    for (std::size_t i = 0; i < ps.size(); i++) {
        out_ << "  mov " << kArgRegs[i] << ", %rax\n";
        store(ps[i].type, ps[i].offset);
    }

    fn.body().accept(*this);

    out_ << "  mov $0, %rax\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov %rbp, %rsp\n";
    out_ << "  pop %rbp\n";
    out_ << "  ret\n";

    if (depth_ != 0) {
        std::fprintf(stderr, "codegen: stack depth %d at the end of %s\n",
                     depth_, fn.name().c_str());
        std::exit(1);
    }
}

void X86_64Linux::run(const Program &program) {
    for (const Function &fn : program) emit(fn);
}
