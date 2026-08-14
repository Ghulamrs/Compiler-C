#include "CodeGen.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

static const char *const kArgRegs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                        "%xmm4", "%xmm5", "%xmm6", "%xmm7" };

void X86_64Linux::push() { out_ << "  push %rax\n"; depth_++; }
void X86_64Linux::pop(const char *reg) { out_ << "  pop " << reg << "\n"; depth_--; }

void X86_64Linux::pushF() {
    out_ << "  sub $8, %rsp\n";
    out_ << "  movsd %xmm0, (%rsp)\n";
    depth_++;
}
void X86_64Linux::popF(const char *reg) {
    out_ << "  movsd (%rsp), " << reg << "\n";
    out_ << "  add $8, %rsp\n";
    depth_--;
}

const char *X86_64Linux::acc(const Type *t) const {
    return t->size(target_) == 8 ? "%rax" : "%eax";
}
const char *X86_64Linux::rhs(const Type *t) const {
    return t->size(target_) == 8 ? "%rdi" : "%edi";
}

void X86_64Linux::canonicalise(const Type *t) {
    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1) out_ << (sign ? "  movsbq %al, %rax\n" : "  movzbq %al, %rax\n");
    else if (sz == 2) out_ << (sign ? "  movswq %ax, %rax\n" : "  movzwq %ax, %rax\n");
    else if (sz == 4) out_ << (sign ? "  movslq %eax, %rax\n" : "  mov %eax, %eax\n");
}

void X86_64Linux::genAddr(const Expr &e) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        if (v->isLocal()) out_ << "  lea -" << v->offset() << "(%rbp), %rax\n";
        else              out_ << "  lea " << v->name() << "(%rip), %rax\n";
        return;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') { u->operand().accept(*this); return; }
    }
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(&e)) {
        if (m->isBitField()) {
            std::fprintf(stderr, "codegen: '%s' is a bit-field and has no address\n",
                         m->name().c_str());
            std::exit(1);
        }
        genAddr(m->object());
        if (m->offset() != 0) out_ << "  add $" << m->offset() << ", %rax\n";
        return;
    }
    if (const StrLit *s = dynamic_cast<const StrLit *>(&e)) {
        out_ << "  lea " << s->label() << "(%rip), %rax\n";
        return;
    }
    // f(x).m and (c ? a : b).m: a struct-valued expression already leaves an
    // address in %rax. Neither is an lvalue, and the parser refuses '&' on both.
    if (const Call *c = dynamic_cast<const Call *>(&e)) {
        if (c->type()->isStructOrUnion()) { c->accept(*this); return; }
    }
    if (const Conditional *q = dynamic_cast<const Conditional *>(&e)) {
        if (q->type()->isStructOrUnion()) { q->accept(*this); return; }
    }
    std::fprintf(stderr, "codegen: this has no address\n");
    std::exit(1);
}

void X86_64Linux::load(const Type *t) {
    if (t->isArray() || t->isStructOrUnion()) return;

    if (t->kind() == Kind::Float)  { out_ << "  movss (%rax), %xmm0\n"; return; }
    if (t->kind() == Kind::Double) { out_ << "  movsd (%rax), %xmm0\n"; return; }

    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  movsbq (%rax), %rax\n" : "  movzbq (%rax), %rax\n");
    else if (sz == 2) out_ << (sign ? "  movswq (%rax), %rax\n" : "  movzwq (%rax), %rax\n");
    else if (sz == 4) out_ << (sign ? "  movslq (%rax), %rax\n" : "  movl (%rax), %eax\n");
    else              out_ << "  movq (%rax), %rax\n";
}

void X86_64Linux::store(const Type *t) {
    if (t->kind() == Kind::Float)  { out_ << "  movss %xmm0, (%rdi)\n"; return; }
    if (t->kind() == Kind::Double) { out_ << "  movsd %xmm0, (%rdi)\n"; return; }

    switch (t->size(target_)) {
    case 1: out_ << "  movb %al, (%rdi)\n"; return;
    case 2: out_ << "  movw %ax, (%rdi)\n"; return;
    case 4: out_ << "  movl %eax, (%rdi)\n"; return;
    default: out_ << "  movq %rax, (%rdi)\n"; return;
    }
}

void X86_64Linux::storeAt(const Type *t, int offset) {
    if (t->kind() == Kind::Float)  { out_ << "  movss %xmm0, -" << offset << "(%rbp)\n"; return; }
    if (t->kind() == Kind::Double) { out_ << "  movsd %xmm0, -" << offset << "(%rbp)\n"; return; }
    switch (t->size(target_)) {
    case 1: out_ << "  movb %al, -"  << offset << "(%rbp)\n"; return;
    case 2: out_ << "  movw %ax, -"  << offset << "(%rbp)\n"; return;
    case 4: out_ << "  movl %eax, -" << offset << "(%rbp)\n"; return;
    default: out_ << "  movq %rax, -" << offset << "(%rbp)\n"; return;
    }
}

void X86_64Linux::visit(const Num &n) {
    if (!n.type()->isFloating()) {
        out_ << "  mov $" << n.value() << ", %rax\n";
        return;
    }
    if (n.type()->kind() == Kind::Float) {
        float f = static_cast<float>(n.dvalue());
        unsigned int bits;
        std::memcpy(&bits, &f, 4);
        out_ << "  mov $" << bits << ", %eax\n";
        out_ << "  movd %eax, %xmm0\n";
    } else {
        double d = n.dvalue();
        unsigned long bits;
        std::memcpy(&bits, &d, 8);
        out_ << "  movabs $" << bits << ", %rax\n";
        out_ << "  movq %rax, %xmm0\n";
    }
}

void X86_64Linux::visit(const Var &n) {
    genAddr(n);
    load(n.type());
}

void X86_64Linux::visit(const StrLit &n) { genAddr(n); }

void X86_64Linux::visit(const MemberAccess &n) {
    if (n.isBitField()) {
        bitFieldUnitAddr(n);
        bitFieldExtract(n);
        return;
    }
    genAddr(n);
    load(n.type());
}

void X86_64Linux::copyBlock(int size) {
    int off = 0;
    while (size - off >= 8) {
        out_ << "  mov " << off << "(%rax), %rcx\n";
        out_ << "  mov %rcx, " << off << "(%rdi)\n";
        off += 8;
    }
    while (size - off >= 4) {
        out_ << "  movl " << off << "(%rax), %ecx\n";
        out_ << "  movl %ecx, " << off << "(%rdi)\n";
        off += 4;
    }
    while (size - off >= 2) {
        out_ << "  movw " << off << "(%rax), %cx\n";
        out_ << "  movw %cx, " << off << "(%rdi)\n";
        off += 2;
    }
    while (size - off >= 1) {
        out_ << "  movb " << off << "(%rax), %cl\n";
        out_ << "  movb %cl, " << off << "(%rdi)\n";
        off += 1;
    }
}

void X86_64Linux::bitFieldUnitAddr(const MemberAccess &m) {
    genAddr(m.object());
    if (m.offset() != 0) out_ << "  add $" << m.offset() << ", %rax\n";
}

void X86_64Linux::bitFieldExtract(const MemberAccess &m) {
    load(m.type());
    int left = 64 - m.bitOffset() - m.width();
    int right = 64 - m.width();
    if (left > 0) out_ << "  shl $" << left << ", %rax\n";
    out_ << (m.type()->isSigned(target_) ? "  sar $" : "  shr $")
         << right << ", %rax\n";
}

void X86_64Linux::bitFieldInsert(const MemberAccess &m) {
    unsigned long ones = (m.width() == 64) ? ~0UL : ((1UL << m.width()) - 1);
    unsigned long mask = ones << m.bitOffset();

    out_ << "  mov %rax, %rdx\n";
    out_ << "  movabs $" << ones << ", %rcx\n";
    out_ << "  and %rcx, %rdx\n";
    if (m.bitOffset() != 0) out_ << "  shl $" << m.bitOffset() << ", %rdx\n";

    out_ << "  push %rax\n";
    out_ << "  mov %rdi, %rax\n";
    load(m.type());
    out_ << "  movabs $" << ~mask << ", %rcx\n";
    out_ << "  and %rcx, %rax\n";
    out_ << "  or %rdx, %rax\n";
    store(m.type());
    out_ << "  pop %rax\n";

    int right = 64 - m.width();
    out_ << "  shl $" << right << ", %rax\n";
    out_ << (m.type()->isSigned(target_) ? "  sar $" : "  shr $")
         << right << ", %rax\n";
}

void X86_64Linux::visit(const Assign &n) {
    const MemberAccess *bf = dynamic_cast<const MemberAccess *>(&n.target());
    if (bf != nullptr && !bf->isBitField()) bf = nullptr;

    if (bf) bitFieldUnitAddr(*bf);
    else    genAddr(n.target());
    push();
    n.value().accept(*this);
    pop("%rdi");

    if (n.type()->isStructOrUnion()) {
        copyBlock(n.type()->size(target_));
        out_ << "  mov %rdi, %rax\n";
        return;
    }
    if (bf) { bitFieldInsert(*bf); return; }

    store(n.type());
    if (!n.type()->isFloating()) canonicalise(n.type());
}

void X86_64Linux::visit(const Postfix &n) {
    genAddr(n.target());
    push();
    load(n.type());

    if (n.type()->isFloating()) {
        pushF();
        bool single = n.type()->kind() == Kind::Float;
        if (single) {
            float one = 1.0f;
            unsigned int bits;
            std::memcpy(&bits, &one, sizeof bits);
            out_ << "  mov $" << bits << ", %eax\n";
            out_ << "  movd %eax, %xmm1\n";
            out_ << (n.increment() ? "  addss %xmm1, %xmm0\n" : "  subss %xmm1, %xmm0\n");
        } else {
            double one = 1.0;
            unsigned long bits;
            std::memcpy(&bits, &one, sizeof bits);
            out_ << "  movabs $" << bits << ", %rax\n";
            out_ << "  movq %rax, %xmm1\n";
            out_ << (n.increment() ? "  addsd %xmm1, %xmm0\n" : "  subsd %xmm1, %xmm0\n");
        }
        popF("%xmm1");
        pop("%rdi");
        store(n.type());
        out_ << "  movapd %xmm1, %xmm0\n";
        return;
    }

    push();
    out_ << (n.increment() ? "  add $" : "  sub $") << n.step() << ", %rax\n";
    pop("%rdx");
    pop("%rdi");
    store(n.type());
    out_ << "  mov %rdx, %rax\n";
}

void X86_64Linux::visit(const Unary &n) {
    if (n.op() == '&') { genAddr(n.operand()); return; }
    if (n.op() == '*') {
        n.operand().accept(*this);
        load(n.type());
        return;
    }

    n.operand().accept(*this);
    if (n.op() == '-' && n.type()->isFloating()) {
        bool isDouble = n.type()->kind() == Kind::Double;
        out_ << "  movq %xmm0, %rax\n";
        out_ << (isDouble ? "  pxor %xmm0, %xmm0\n" : "  pxor %xmm0, %xmm0\n");
        out_ << "  movq %rax, %xmm1\n";
        out_ << (isDouble ? "  subsd %xmm1, %xmm0\n" : "  subss %xmm1, %xmm0\n");
    } else if (n.op() == '-') {
        out_ << "  neg " << acc(n.type()) << "\n";
        canonicalise(n.type());
    } else if (n.op() == '!' && n.operand().type()->isFloating()) {
        bool isDouble = n.operand().type()->kind() == Kind::Double;
        out_ << "  pxor %xmm1, %xmm1\n";
        out_ << (isDouble ? "  ucomisd %xmm1, %xmm0\n" : "  ucomiss %xmm1, %xmm0\n");
        out_ << "  sete %al\n";
        out_ << "  movzbq %al, %rax\n";
    } else if (n.op() == '!') {
        out_ << "  cmp $0, %rax\n";
        out_ << "  sete %al\n";
        out_ << "  movzbq %al, %rax\n";
    }
}

void X86_64Linux::genConversion(const Type *from, const Type *to) {
    if (to->isVoid()) return;

    bool fromF = from->isFloating(), toF = to->isFloating();

    if (!fromF && !toF) { canonicalise(to); return; }

    if (fromF && toF) {
        if (from->kind() == to->kind()) return;
        if (to->kind() == Kind::Double) out_ << "  cvtss2sd %xmm0, %xmm0\n";
        else                            out_ << "  cvtsd2ss %xmm0, %xmm0\n";
        return;
    }

    if (!fromF && toF) {
        const char *op = to->kind() == Kind::Double ? "cvtsi2sdq" : "cvtsi2ssq";
        out_ << "  " << op << " %rax, %xmm0\n";
        return;
    }

    const char *op = from->kind() == Kind::Double ? "cvttsd2si" : "cvttss2si";
    out_ << "  " << op << " %xmm0, %rax\n";
    canonicalise(to);
}

void X86_64Linux::visit(const Cast &n) {
    n.value().accept(*this);
    genConversion(n.value().type(), n.type());
}

void X86_64Linux::genFloatBinary(const Binary &n) {
    const Type *t = n.lhs().type();
    bool isDouble = t->kind() == Kind::Double;
    const char *sfx = isDouble ? "sd" : "ss";

    n.rhs().accept(*this);
    pushF();
    n.lhs().accept(*this);
    popF("%xmm1");

    switch (n.op()) {
    case BinOp::Add: out_ << "  add" << sfx << " %xmm1, %xmm0\n"; return;
    case BinOp::Sub: out_ << "  sub" << sfx << " %xmm1, %xmm0\n"; return;
    case BinOp::Mul: out_ << "  mul" << sfx << " %xmm1, %xmm0\n"; return;
    case BinOp::Div: out_ << "  div" << sfx << " %xmm1, %xmm0\n"; return;
    default: break;
    }

    const char *set = nullptr;
    switch (n.op()) {
    case BinOp::Eq: set = "sete";  break;
    case BinOp::Ne: set = "setne"; break;
    case BinOp::Lt: set = "setb";  break;
    case BinOp::Le: set = "setbe"; break;
    case BinOp::Gt: set = "seta";  break;
    case BinOp::Ge: set = "setae"; break;
    default:
        std::fprintf(stderr, "codegen: that operator has no floating form\n");
        std::exit(1);
    }
    out_ << "  ucomi" << sfx << " %xmm1, %xmm0\n";
    out_ << "  " << set << " %al\n";
    out_ << "  movzbq %al, %rax\n";
}

void X86_64Linux::visit(const Binary &n) {
    if (n.op() == BinOp::LAnd || n.op() == BinOp::LOr) {
        int id = nextLabel();
        bool isAnd = n.op() == BinOp::LAnd;
        const char *shortJump = isAnd ? "je" : "jne";

        genTruth(n.lhs());
        out_ << "  cmp $0, %rax\n";
        out_ << "  " << shortJump << " " << label("sc", id) << "\n";
        genTruth(n.rhs());
        out_ << "  cmp $0, %rax\n";
        out_ << "  " << shortJump << " " << label("sc", id) << "\n";
        out_ << "  mov $" << (isAnd ? 1 : 0) << ", %rax\n";
        out_ << "  jmp " << label("scend", id) << "\n";
        out_ << label("sc", id) << ":\n";
        out_ << "  mov $" << (isAnd ? 0 : 1) << ", %rax\n";
        out_ << label("scend", id) << ":\n";
        return;
    }

    if (n.lhs().type()->isFloating()) { genFloatBinary(n); return; }

    n.rhs().accept(*this);
    push();
    n.lhs().accept(*this);
    pop("%rdi");

    const Type *t = n.lhs().type();
    const char *a = acc(t);
    const char *d = rhs(t);
    bool sign = t->isSigned(target_);
    bool wide = t->size(target_) == 8;

    switch (n.op()) {
    case BinOp::Add: out_ << "  add "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::Sub: out_ << "  sub "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::Mul: out_ << "  imul " << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::BitAnd: out_ << "  and " << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::BitOr:  out_ << "  or "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::BitXor: out_ << "  xor " << d << ", " << a << "\n"; canonicalise(n.type()); return;

    case BinOp::Div:
    case BinOp::Mod:
        if (sign) out_ << (wide ? "  cqo\n" : "  cdq\n");
        else      out_ << "  xor %edx, %edx\n";
        out_ << (sign ? "  idiv " : "  div ") << d << "\n";
        if (n.op() == BinOp::Mod) out_ << (wide ? "  mov %rdx, %rax\n" : "  mov %edx, %eax\n");
        canonicalise(n.type());
        return;

    case BinOp::Shl:
    case BinOp::Shr:
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
    std::vector<std::vector<bool> > isSse;
    std::vector<std::vector<int> > slot;
    std::vector<bool> onStack;
    // A MEMORY-class return spends %rdi on the hidden pointer before any
    // argument is placed, so every integer argument shifts along by one.
    bool sret = n.type()->isStructOrUnion() && n.type()->size(target_) > 16;
    int ints = sret ? 1 : 0, sses = 0;
    int stackSlots = 0;
    for (const ExprPtr &arg : n.args()) {
        const Type *t = arg->type();
        std::vector<bool> lanes;
        bool memory = t->isStructOrUnion() && t->size(target_) > 16;
        if (!memory) {
            if (t->isStructOrUnion()) lanes = classifyEightbytes(t, target_);
            else                      lanes.push_back(t->isFloating());
            int wantInt = 0, wantSse = 0;
            for (bool sse : lanes) { if (sse) wantSse++; else wantInt++; }
            memory = ints + wantInt > 6 || sses + wantSse > 8;
        }

        std::vector<int> regs;
        if (memory) {
            lanes.clear();
            int size = t->isStructOrUnion() ? t->size(target_) : 8;
            stackSlots += (size + 7) / 8;
        } else {
            for (bool sse : lanes) regs.push_back(sse ? sses++ : ints++);
        }
        onStack.push_back(memory);
        isSse.push_back(lanes);
        slot.push_back(regs);
    }

    // Counted before anything is pushed: the memory arguments sit on the stack
    // the call has to find aligned, so deciding this afterwards is wrong.
    int padSlots = ((depth_ + stackSlots) % 2 != 0) ? 1 : 0;
    if (padSlots) { out_ << "  sub $8, %rsp\n"; depth_++; }

    // Reverse: push moves down, and the first memory argument must end lowest.
    for (std::size_t i = n.args().size(); i-- > 0; ) {
        if (!onStack[i]) continue;
        const Type *t = n.args()[i]->type();
        n.args()[i]->accept(*this);
        if (!t->isStructOrUnion()) {
            if (t->isFloating()) pushF(); else push();
            continue;
        }
        int size = t->size(target_);
        int slots = (size + 7) / 8;
        out_ << "  mov %rax, %rcx\n";
        for (int k = slots; k-- > 0; ) {
            int off = k * 8;
            int left = size - off;
            if (left >= 8)      out_ << "  mov "   << off << "(%rcx), %rax\n";
            else if (left >= 4) out_ << "  movl "  << off << "(%rcx), %eax\n";
            else if (left >= 2) out_ << "  movzwl "<< off << "(%rcx), %eax\n";
            else                out_ << "  movzbl "<< off << "(%rcx), %eax\n";
            push();
        }
    }

    if (n.callee() != nullptr) {
        n.callee()->accept(*this);
        push();
    }

    for (const ExprPtr &arg : n.args()) {
        if (onStack[&arg - &n.args()[0]]) continue;
        arg->accept(*this);
        if (arg->type()->isFloating()) pushF(); else push();
    }
    for (std::size_t i = n.args().size(); i-- > 0; ) {
        if (onStack[i]) continue;
        const Type *t = n.args()[i]->type();
        if (!t->isStructOrUnion()) {
            if (isSse[i][0]) popF(kSseRegs[slot[i][0]]);
            else             pop(kArgRegs[slot[i][0]]);
            continue;
        }
        pop("%rax");
        int size = t->size(target_);
        for (std::size_t k = 0; k < isSse[i].size(); k++) {
            int off = static_cast<int>(k) * 8;
            int left = size - off;
            if (isSse[i][k]) {
                out_ << (left >= 8 ? "  movsd " : "  movss ") << off
                     << "(%rax), " << kSseRegs[slot[i][k]] << "\n";
            } else if (left >= 8) {
                out_ << "  mov " << off << "(%rax), " << kArgRegs[slot[i][k]] << "\n";
            } else {
                if (left >= 4)      out_ << "  movl "   << off << "(%rax), %ecx\n";
                else if (left >= 2) out_ << "  movzwl " << off << "(%rax), %ecx\n";
                else                out_ << "  movzbl " << off << "(%rax), %ecx\n";
                out_ << "  mov %rcx, " << kArgRegs[slot[i][k]] << "\n";
            }
        }
    }
    // %r11, not %rax: %rax is written just below with the variadic SSE count.
    if (n.callee() != nullptr) pop("%r11");

    if (sret) out_ << "  lea " << (-n.resultSlot()) << "(%rbp), %rdi\n";

    out_ << "  mov $" << (n.isVariadic() ? sses : 0) << ", %rax\n";
    if (n.callee() != nullptr) out_ << "  call *%r11\n";
    else                       out_ << "  call " << n.name() << "\n";

    if (stackSlots + padSlots > 0) {
        out_ << "  add $" << (stackSlots + padSlots) * 8 << ", %rsp\n";
        depth_ -= stackSlots + padSlots;
    }

    if (sret) {
        out_ << "  lea " << (-n.resultSlot()) << "(%rbp), %rax\n";
        return;
    }

    if (n.type()->isStructOrUnion()) {
        std::vector<bool> lanes = classifyEightbytes(n.type(), target_);
        int size = n.type()->size(target_);
        int base = n.resultSlot();
        const char *ret[2] = { "%rax", "%rdx" };
        const char *sret[2] = { "%xmm0", "%xmm1" };
        int nextInt = 0, nextSse = 0;
        for (std::size_t k = 0; k < lanes.size(); k++) {
            int off = static_cast<int>(k) * 8 - base;
            int left = size - static_cast<int>(k) * 8;
            if (lanes[k]) {
                out_ << (left >= 8 ? "  movsd " : "  movss ") << sret[nextSse++]
                     << ", " << off << "(%rbp)\n";
            } else {
                const char *r = ret[nextInt++];
                if (left >= 8)      out_ << "  mov "  << r << ", " << off << "(%rbp)\n";
                else if (left >= 4) out_ << "  movl %" << (r[1] == 'a' ? "eax" : "edx")
                                         << ", " << off << "(%rbp)\n";
                else if (left >= 2) out_ << "  movw %" << (r[1] == 'a' ? "ax" : "dx")
                                         << ", " << off << "(%rbp)\n";
                else                out_ << "  movb %" << (r[1] == 'a' ? "al" : "dl")
                                         << ", " << off << "(%rbp)\n";
            }
        }
        out_ << "  lea " << (-base) << "(%rbp), %rax\n";
        return;
    }

    if (!n.type()->isVoid() && !n.type()->isFloating()) canonicalise(n.type());
}

void X86_64Linux::genTruth(const Expr &e) {
    e.accept(*this);
    if (!e.type()->isFloating()) return;
    bool isDouble = e.type()->kind() == Kind::Double;
    out_ << "  pxor %xmm1, %xmm1\n";
    out_ << (isDouble ? "  ucomisd %xmm1, %xmm0\n" : "  ucomiss %xmm1, %xmm0\n");
    out_ << "  setne %al\n";
    out_ << "  movzbq %al, %rax\n";
}

void X86_64Linux::visit(const ExprStmt &n) { n.expr().accept(*this); }

void X86_64Linux::visit(const Return &n) {
    n.value().accept(*this);

    if (sretSlot_ != 0) {
        out_ << "  mov -" << sretSlot_ << "(%rbp), %rdi\n";
        copyBlock(n.value().type()->size(target_));
        out_ << "  mov -" << sretSlot_ << "(%rbp), %rax\n";
        out_ << "  jmp " << returnLabel_ << "\n";
        return;
    }

    if (n.value().type()->isStructOrUnion()) {
        const Type *t = n.value().type();
        std::vector<bool> lanes = classifyEightbytes(t, target_);
        int size = t->size(target_);
        const char *ret[2] = { "%rax", "%rdx" };
        const char *sret[2] = { "%xmm0", "%xmm1" };
        int nextInt = 0, nextSse = 0;
        out_ << "  mov %rax, %rcx\n";
        for (std::size_t k = 0; k < lanes.size(); k++) {
            int off = static_cast<int>(k) * 8;
            int left = size - off;
            if (lanes[k]) {
                out_ << (left >= 8 ? "  movsd " : "  movss ") << off
                     << "(%rcx), " << sret[nextSse++] << "\n";
            } else if (left >= 8) {
                out_ << "  mov " << off << "(%rcx), " << ret[nextInt++] << "\n";
            } else {
                const char *r = ret[nextInt++];
                const char *e = r[1] == 'a' ? "%eax" : "%edx";
                if (left >= 4)      out_ << "  movl "   << off << "(%rcx), " << e << "\n";
                else if (left >= 2) out_ << "  movzwl " << off << "(%rcx), " << e << "\n";
                else                out_ << "  movzbl " << off << "(%rcx), " << e << "\n";
            }
        }
    }
    out_ << "  jmp " << returnLabel_ << "\n";
}

void X86_64Linux::visit(const Block &n) {
    for (const StmtPtr &s : n.body()) s->accept(*this);
}

void X86_64Linux::visit(const If &n) {
    int id = nextLabel();
    genTruth(n.cond());
    out_ << "  cmp $0, %rax\n";
    if (n.elseArm()) {
        out_ << "  je " << label("else", id) << "\n";
        n.thenArm().accept(*this);
        out_ << "  jmp " << label("end", id) << "\n";
        out_ << label("else", id) << ":\n";
        n.elseArm()->accept(*this);
    } else {
        out_ << "  je " << label("end", id) << "\n";
        n.thenArm().accept(*this);
    }
    out_ << label("end", id) << ":\n";
}

void X86_64Linux::visit(const While &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("begin", id) });
    out_ << label("begin", id) << ":\n";
    genTruth(n.cond());
    out_ << "  cmp $0, %rax\n";
    out_ << "  je " << label("end", id) << "\n";
    n.body().accept(*this);
    out_ << "  jmp " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";
    jumps_.pop_back();
}

void X86_64Linux::visit(const For &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    if (n.init()) n.init()->accept(*this);
    out_ << label("begin", id) << ":\n";
    if (n.cond()) {
        genTruth(*n.cond());
        out_ << "  cmp $0, %rax\n";
        out_ << "  je " << label("end", id) << "\n";
    }
    n.body().accept(*this);
    out_ << label("step", id) << ":\n";
    if (n.step()) n.step()->accept(*this);
    out_ << "  jmp " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";

    jumps_.pop_back();
}

void X86_64Linux::visit(const DoWhile &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    out_ << label("begin", id) << ":\n";
    n.body().accept(*this);
    out_ << label("step", id) << ":\n";
    genTruth(n.cond());
    out_ << "  cmp $0, %rax\n";
    out_ << "  jne " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";

    jumps_.pop_back();
}

void X86_64Linux::visit(const Switch &n) {
    int id = nextLabel();

    n.cond().accept(*this);
    for (const Case *c : n.cases()) {
        long v = c->value();
        if (v >= -2147483648L && v <= 2147483647L) {
            out_ << "  cmp $" << v << ", %rax\n";
        } else {
            out_ << "  movabs $" << v << ", %rdx\n";
            out_ << "  cmp %rdx, %rax\n";
        }
        out_ << "  je " << label("case", c->id()) << "\n";
    }
    out_ << "  jmp "
         << (n.defaultCase() ? label("default", n.defaultCase()->id())
                             : label("end", id))
         << "\n";

    jumps_.push_back({ label("end", id), "" });
    n.body().accept(*this);
    jumps_.pop_back();
    out_ << label("end", id) << ":\n";
}

void X86_64Linux::visit(const Case &n) {
    out_ << label(n.isDefault() ? "default" : "case", n.id()) << ":\n";
    n.body().accept(*this);
}

void X86_64Linux::visit(const Goto &n) {
    out_ << "  jmp " << userLabel(n.label()) << "\n";
}

void X86_64Linux::visit(const Label &n) {
    out_ << userLabel(n.name()) << ":\n";
    n.body().accept(*this);
}

void X86_64Linux::visit(const Conditional &n) {
    int id = nextLabel();
    genTruth(n.cond());
    out_ << "  cmp $0, %rax\n";
    out_ << "  je " << label("else", id) << "\n";
    n.thenArm().accept(*this);
    out_ << "  jmp " << label("end", id) << "\n";
    out_ << label("else", id) << ":\n";
    n.elseArm().accept(*this);
    out_ << label("end", id) << ":\n";
}

void X86_64Linux::visit(const Comma &n) {
    n.left().accept(*this);
    n.right().accept(*this);
}

void X86_64Linux::visit(const Break &) {
    out_ << "  jmp " << jumps_.back().brk << "\n";
}

void X86_64Linux::visit(const Continue &) {
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            out_ << "  jmp " << jumps_[i].cont << "\n";
            return;
        }
    }
}

std::string X86_64Linux::label(const char *kind, int id) const {
    return labelPrefix_ + kind + "." + std::to_string(id);
}

std::string X86_64Linux::userLabel(const std::string &name) const {
    return labelPrefix_ + "user." + name;
}

void X86_64Linux::finishChunk() {
    chunks_.push_back(out_.str());
    out_.str(std::string());
    out_.clear();
}

void X86_64Linux::emit(const Function &fn) {
    depth_ = 0;
    labels_ = 0;
    labelPrefix_ = ".L." + fn.name() + ".";
    returnLabel_ = ".L.return." + fn.name();

    if (!fn.isStatic()) out_ << "  .globl " << fn.name() << "\n";
    out_ << "  .text\n";
    out_ << fn.name() << ":\n";
    out_ << "  push %rbp\n";
    out_ << "  mov %rsp, %rbp\n";
    if (fn.frameSize() > 0) out_ << "  sub $" << fn.frameSize() << ", %rsp\n";

    sretSlot_ = fn.sretSlot();
    if (sretSlot_ != 0) out_ << "  mov %rdi, -" << sretSlot_ << "(%rbp)\n";

    const std::vector<Param> &ps = fn.params();
    // Starts at 1 for a MEMORY return, exactly as the caller's count does.
    int ints = (sretSlot_ != 0) ? 1 : 0, sses = 0;
    int stackAt = 16;
    for (std::size_t i = 0; i < ps.size(); i++) {
        const Type *pt = ps[i].type;
        bool memory = pt->isStructOrUnion() && pt->size(target_) > 16;
        if (!memory) {
            std::vector<bool> lanes;
            if (pt->isStructOrUnion()) lanes = classifyEightbytes(pt, target_);
            else                       lanes.push_back(pt->isFloating());
            int wantInt = 0, wantSse = 0;
            for (bool sse : lanes) { if (sse) wantSse++; else wantInt++; }
            memory = ints + wantInt > 6 || sses + wantSse > 8;
        }
        if (memory) {
            int size = pt->size(target_);
            int slots = (size + 7) / 8;
            for (int k = 0; k < slots; k++) {
                int from = stackAt + k * 8;
                int to = k * 8 - ps[i].offset;
                int left = size - k * 8;
                if (left >= 8) {
                    out_ << "  mov " << from << "(%rbp), %rax\n";
                    out_ << "  movq %rax, " << to << "(%rbp)\n";
                } else if (left >= 4) {
                    out_ << "  movl " << from << "(%rbp), %eax\n";
                    out_ << "  movl %eax, " << to << "(%rbp)\n";
                } else if (left >= 2) {
                    out_ << "  movzwl " << from << "(%rbp), %eax\n";
                    out_ << "  movw %ax, " << to << "(%rbp)\n";
                } else {
                    out_ << "  movzbl " << from << "(%rbp), %eax\n";
                    out_ << "  movb %al, " << to << "(%rbp)\n";
                }
            }
            stackAt += slots * 8;
            continue;
        }

        if (ps[i].type->isStructOrUnion()) {
            std::vector<bool> lanes = classifyEightbytes(ps[i].type, target_);
            int size = ps[i].type->size(target_);
            for (std::size_t k = 0; k < lanes.size(); k++) {
                int off = static_cast<int>(k) * 8 - ps[i].offset;
                int left = size - static_cast<int>(k) * 8;
                if (lanes[k]) {
                    out_ << (left >= 8 ? "  movsd " : "  movss ") << kSseRegs[sses++]
                         << ", " << off << "(%rbp)\n";
                } else {
                    out_ << "  mov " << kArgRegs[ints++] << ", %rax\n";
                    if (left >= 8)      out_ << "  movq %rax, "  << off << "(%rbp)\n";
                    else if (left >= 4) out_ << "  movl %eax, "  << off << "(%rbp)\n";
                    else if (left >= 2) out_ << "  movw %ax, "   << off << "(%rbp)\n";
                    else                out_ << "  movb %al, "   << off << "(%rbp)\n";
                }
            }
            continue;
        }
        if (ps[i].type->isFloating()) {
            out_ << "  movsd " << kSseRegs[sses++] << ", %xmm0\n";
        } else {
            out_ << "  mov " << kArgRegs[ints++] << ", %rax\n";
        }
        storeAt(ps[i].type, ps[i].offset);
    }

    fn.body().accept(*this);

    if (sretSlot_ != 0)                  out_ << "  mov -" << sretSlot_ << "(%rbp), %rax\n";
    else if (fn.returns()->isFloating()) out_ << "  pxor %xmm0, %xmm0\n";
    else                                 out_ << "  mov $0, %rax\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov %rbp, %rsp\n";
    out_ << "  pop %rbp\n";
    out_ << "  ret\n";

    if (depth_ != 0) {
        std::fprintf(stderr, "codegen: stack depth %d at the end of %s\n",
                     depth_, fn.name().c_str());
        std::exit(1);
    }
    finishChunk();
}

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
            if (!g.isStatic) out_ << "  .globl " << g.name << "\n";
            out_ << "  .align " << g.type->align(target_) << "\n";
            out_ << g.name << ":\n";
            if (!g.hasInit) { out_ << "  .zero " << size << "\n"; continue; }

            int at = 0;
            for (const GlobalPiece &p : g.init) {
                if (p.offset > at) out_ << "  .zero " << (p.offset - at) << "\n";
                switch (p.size) {
                case 1: out_ << "  .byte "  << p.value << "\n"; break;
                case 2: out_ << "  .word "  << p.value << "\n"; break;
                case 4: out_ << "  .long "  << p.value << "\n"; break;
                default: out_ << "  .quad " << p.value << "\n"; break;
                }
                at = p.offset + p.size;
            }
            if (at < size) out_ << "  .zero " << (size - at) << "\n";
        }
    }
}

void X86_64Linux::run(const Program &program) {
    emitData(program);
    finishChunk();
    for (const Function &fn : program.functions) emit(fn);

    for (const std::string &chunk : chunks_) sink_ << chunk;
}
