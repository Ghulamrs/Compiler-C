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

// The address of a place. A local is an offset from the frame pointer, a
// global is a symbol reached through %rip, a dereference is simply the value
// of the pointer, and a string literal is its label. Everything that can be
// assigned to is one of those four.
void X86_64Linux::genAddr(const Expr &e) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        if (v->isLocal()) out_ << "  lea -" << v->offset() << "(%rbp), %rax\n";
        else              out_ << "  lea " << v->name() << "(%rip), %rax\n";
        return;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') { u->operand().accept(*this); return; }
    }
    if (const StrLit *s = dynamic_cast<const StrLit *>(&e)) {
        out_ << "  lea " << s->label() << "(%rip), %rax\n";
        return;
    }
    std::fprintf(stderr, "codegen: this has no address\n");
    std::exit(1);
}

void X86_64Linux::load(const Type *t) {
    // An array does not load. Its value is its address, which is already here -
    // that is decay, expressed in one instruction that does not exist.
    if (t->isArray()) return;

    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  movsbq (%rax), %rax\n" : "  movzbq (%rax), %rax\n");
    else if (sz == 2) out_ << (sign ? "  movswq (%rax), %rax\n" : "  movzwq (%rax), %rax\n");
    else if (sz == 4) out_ << (sign ? "  movslq (%rax), %rax\n" : "  movl (%rax), %eax\n");
    else              out_ << "  movq (%rax), %rax\n";
}

void X86_64Linux::store(const Type *t) {
    switch (t->size(target_)) {
    case 1: out_ << "  movb %al, (%rdi)\n"; return;
    case 2: out_ << "  movw %ax, (%rdi)\n"; return;
    case 4: out_ << "  movl %eax, (%rdi)\n"; return;
    default: out_ << "  movq %rax, (%rdi)\n"; return;
    }
}

// ---- expressions ----

void X86_64Linux::visit(const Num &n) {
    out_ << "  mov $" << n.value() << ", %rax\n";
}

void X86_64Linux::visit(const Var &n) {
    genAddr(n);
    load(n.type());
}

void X86_64Linux::visit(const StrLit &n) { genAddr(n); }

void X86_64Linux::visit(const Assign &n) {
    // The address first, and kept on the stack: computing the value can call a
    // function, and anything left in a register would not survive it.
    genAddr(n.target());
    push();
    n.value().accept(*this);
    pop("%rdi");
    store(n.type());
    // The stored value is the value of the expression, and it must read back
    // as the narrower type would: char c; (c = 300) is 44, not 300.
    canonicalise(n.type());
}

void X86_64Linux::visit(const Unary &n) {
    // '&' reads nothing: it wants the address, not the value there.
    if (n.op() == '&') { genAddr(n.operand()); return; }
    // '*' names a place. The operand's value is that address, so evaluating it
    // gives the address and the load turns it into a value - unless the result
    // is an array, which stays an address.
    if (n.op() == '*') {
        n.operand().accept(*this);
        load(n.type());
        return;
    }

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
        out_ << "  lea -" << ps[i].offset << "(%rbp), %rdi\n";
        store(ps[i].type);
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

// Strings are emitted as bytes rather than with .string, so nothing in the
// text has to be escaped for the assembler - a quote or a backslash in a C
// string would otherwise have to be re-escaped correctly on the way out.
void X86_64Linux::emitData(const Program &program) {
    if (!program.strings.empty()) {
        out_ << "  .section .rodata\n";
        for (const auto &s : program.strings) {
            out_ << s.first << ":\n";
            out_ << "  .byte ";
            for (unsigned char c : s.second) out_ << static_cast<int>(c) << ", ";
            out_ << "0\n";
        }
    }

    if (!program.globals.empty()) {
        out_ << "  .data\n";
        for (const Global &g : program.globals) {
            int size = g.type->size(target_);
            // static means internal linkage, which is the absence of .globl.
            if (!g.isStatic) out_ << "  .globl " << g.name << "\n";
            out_ << "  .align " << g.type->align(target_) << "\n";
            out_ << g.name << ":\n";
            if (!g.hasInit) { out_ << "  .zero " << size << "\n"; continue; }
            switch (size) {
            case 1: out_ << "  .byte "  << g.init << "\n"; break;
            case 2: out_ << "  .word "  << g.init << "\n"; break;
            case 4: out_ << "  .long "  << g.init << "\n"; break;
            default: out_ << "  .quad " << g.init << "\n"; break;
            }
        }
    }
}

void X86_64Linux::run(const Program &program) {
    emitData(program);
    for (const Function &fn : program.functions) emit(fn);
}
