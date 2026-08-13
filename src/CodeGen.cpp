#include "CodeGen.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

static const char *const kArgRegs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
// System V classifies each argument INTEGER or SSE and counts the two lanes
// separately: the first six integers go in the registers above and the first
// eight floating values in these, independently of one another.
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                        "%xmm4", "%xmm5", "%xmm6", "%xmm7" };

void X86_64Linux::push() { out_ << "  push %rax\n"; depth_++; }
void X86_64Linux::pop(const char *reg) { out_ << "  pop " << reg << "\n"; depth_--; }

// An %xmm register cannot be pushed, so the slot is made and filled by hand.
// depth_ still counts it, because what it is really counting is how far %rsp
// has moved - which is what the call alignment depends on.
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
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(&e)) {
        // A bit-field has no address, and handing back the address of its
        // storage unit here would be worse than refusing: every caller would
        // then be reading and writing its neighbours as well. The parser
        // refuses '&' on one, so reaching this is a bug in the compiler rather
        // than in the program being compiled.
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
    std::fprintf(stderr, "codegen: this has no address\n");
    std::exit(1);
}

void X86_64Linux::load(const Type *t) {
    // An array does not load. Its value is its address, which is already here -
    // that is decay, expressed in one instruction that does not exist. A struct
    // is the same: no register holds one, so what travels is where it lives.
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

// ---- expressions ----

void X86_64Linux::visit(const Num &n) {
    if (!n.type()->isFloating()) {
        out_ << "  mov $" << n.value() << ", %rax\n";
        return;
    }
    // The bit pattern, moved across rather than loaded from memory: it saves a
    // .rodata entry per constant and there is no immediate form for %xmm.
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

// Unrolled rather than a loop: the size is known, and a handful of moves reads
// better in the output than a counter would. %rcx is the scratch; nothing else
// is live by the time an assignment stores.
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

// The address of the storage unit the field lives in. Not the field's address -
// it does not have one - which is why this is spelled out here rather than
// being genAddr with an exception in it.
void X86_64Linux::bitFieldUnitAddr(const MemberAccess &m) {
    genAddr(m.object());
    if (m.offset() != 0) out_ << "  add $" << m.offset() << ", %rax\n";
}

// Two shifts and nothing else. Left until the field's top bit is the register's
// top bit, then right until it is back at the bottom - arithmetically if the
// member is signed, so a 3-bit field holding 7 reads as -1 when it is signed and
// as 7 when it is not. Shifting is what makes the sign work out; masking would
// need a separate sign-extension step.
void X86_64Linux::bitFieldExtract(const MemberAccess &m) {
    load(m.type());                       // the whole unit, from the address in %rax
    int left = 64 - m.bitOffset() - m.width();
    int right = 64 - m.width();
    if (left > 0) out_ << "  shl $" << left << ", %rax\n";
    out_ << (m.type()->isSigned(target_) ? "  sar $" : "  shr $")
         << right << ", %rax\n";
}

// Read, modify, write. The neighbours in the unit have to survive, which is the
// whole difficulty: a bit-field cannot be stored, only merged.
void X86_64Linux::bitFieldInsert(const MemberAccess &m) {
    // %rax holds the value to store, %rdi the address of the unit.
    unsigned long ones = (m.width() == 64) ? ~0UL : ((1UL << m.width()) - 1);
    unsigned long mask = ones << m.bitOffset();

    out_ << "  mov %rax, %rdx\n";                     // keep the value
    out_ << "  movabs $" << ones << ", %rcx\n";
    out_ << "  and %rcx, %rdx\n";                     // drop anything too wide
    if (m.bitOffset() != 0) out_ << "  shl $" << m.bitOffset() << ", %rdx\n";

    out_ << "  push %rax\n";
    out_ << "  mov %rdi, %rax\n";
    load(m.type());                                   // the unit as it stands
    out_ << "  movabs $" << ~mask << ", %rcx\n";
    out_ << "  and %rcx, %rax\n";                     // clear just this field
    out_ << "  or %rdx, %rax\n";                      // and put it back
    store(m.type());
    out_ << "  pop %rax\n";

    // The value of the assignment is what a read of the field would now give,
    // not what was handed in: (f.a = 300) on a 3-bit field is 4, not 300.
    int right = 64 - m.width();
    out_ << "  shl $" << right << ", %rax\n";
    out_ << (m.type()->isSigned(target_) ? "  sar $" : "  shr $")
         << right << ", %rax\n";
}

void X86_64Linux::visit(const Assign &n) {
    // A bit-field is merged into its storage unit rather than stored over it,
    // so it takes the unit's address and its own path below.
    const MemberAccess *bf = dynamic_cast<const MemberAccess *>(&n.target());
    if (bf != nullptr && !bf->isBitField()) bf = nullptr;

    // The address first, and kept on the stack: computing the value can call a
    // function, and anything left in a register would not survive it.
    if (bf) bitFieldUnitAddr(*bf);
    else    genAddr(n.target());
    push();
    n.value().accept(*this);
    pop("%rdi");

    // A whole struct is copied rather than stored: the value in %rax is the
    // address of the source, not the object itself.
    if (n.type()->isStructOrUnion()) {
        copyBlock(n.type()->size(target_));
        out_ << "  mov %rdi, %rax\n";   // the assignment's value is the object
        return;
    }
    if (bf) { bitFieldInsert(*bf); return; }

    store(n.type());
    // The stored value is the value of the expression, and it must read back
    // as the narrower type would: char c; (c = 300) is 44, not 300. Floating
    // values have no narrowing of that kind and are already in %xmm0.
    if (!n.type()->isFloating()) canonicalise(n.type());
}

// x++ and x--.
//
// The old value has to survive the store, and there is no register that can be
// relied on to hold it across one - so it goes on the stack under the address,
// which is already there. Three things are in flight and the stack keeps two of
// them: the address beneath, the old value above it, and the new value in the
// register the arithmetic left it in.
void X86_64Linux::visit(const Postfix &n) {
    genAddr(n.target());
    push();                       // the address, beneath everything
    load(n.type());               // the old value, in %rax or %xmm0

    if (n.type()->isFloating()) {
        pushF();                  // the old value, above the address
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
        popF("%xmm1");            // the old value back
        pop("%rdi");              // the address
        store(n.type());          // the new value, from %xmm0
        out_ << "  movapd %xmm1, %xmm0\n";   // and the expression is the old one
        return;
    }

    push();                       // the old value, above the address
    out_ << (n.increment() ? "  add $" : "  sub $") << n.step() << ", %rax\n";
    pop("%rdx");                  // the old value back
    pop("%rdi");                  // the address
    store(n.type());              // the new value, from %rax
    out_ << "  mov %rdx, %rax\n";  // and the expression is the old one
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
    if (n.op() == '-' && n.type()->isFloating()) {
        // Subtracting from zero rather than flipping the sign bit: shorter to
        // write, and the sign of zero is not something the language exposes yet.
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

// Every conversion between the two register files, and within each. The parser
// decided that this conversion happens; this only performs it.
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
        // The integer is canonically extended to 64 bits already, so one
        // signed 64-bit conversion covers every signed source width.
        // An unsigned 64-bit source would need more than this; nothing can
        // produce one yet, and it is refused in the parser rather than
        // silently converted wrongly.
        const char *op = to->kind() == Kind::Double ? "cvtsi2sdq" : "cvtsi2ssq";
        out_ << "  " << op << " %rax, %xmm0\n";
        return;
    }

    // Floating to integer truncates towards zero, which is what C says - not
    // rounds. (int)1.9 is 1 and (int)-1.9 is -1.
    const char *op = from->kind() == Kind::Double ? "cvttsd2si" : "cvttss2si";
    out_ << "  " << op << " %xmm0, %rax\n";
    canonicalise(to);
}

void X86_64Linux::visit(const Cast &n) {
    n.value().accept(*this);
    genConversion(n.value().type(), n.type());
}

// Arithmetic and comparison in %xmm. Comparison is the interesting half:
// ucomis sets the flags the way an unsigned integer comparison would, so the
// unsigned condition codes are the correct ones even though the values are
// signed. NaN makes every comparison false except !=, which this does not yet
// distinguish - nothing can produce a NaN in the language today.
void X86_64Linux::genFloatBinary(const Binary &n) {
    const Type *t = n.lhs().type();
    bool isDouble = t->kind() == Kind::Double;
    const char *sfx = isDouble ? "sd" : "ss";

    n.rhs().accept(*this);
    pushF();
    n.lhs().accept(*this);
    popF("%xmm1");
    // %xmm0 = lhs, %xmm1 = rhs

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
    // Short circuit, and therefore branches rather than the push/pop pattern:
    // the right side must not be evaluated when the left has already decided
    // the answer. "0 && putchar(65)" prints nothing, and that is observable.
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

    // The operands share a type by now, so one side decides which register
    // file this operation lives in.
    if (n.lhs().type()->isFloating()) { genFloatBinary(n); return; }

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
    case BinOp::BitAnd: out_ << "  and " << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::BitOr:  out_ << "  or "  << d << ", " << a << "\n"; canonicalise(n.type()); return;
    case BinOp::BitXor: out_ << "  xor " << d << ", " << a << "\n"; canonicalise(n.type()); return;

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
    // Classify first. Each argument goes in the integer lane or the SSE lane,
    // and the two are numbered independently: f(1, 1.5, 2) puts 1 in %rdi,
    // 2 in %rsi and 1.5 in %xmm0.
    // One entry per argument, and for a struct one per eightbyte of it: System
    // V spends a register on each, from whichever file the classification says.
    std::vector<std::vector<bool> > isSse;
    std::vector<std::vector<int> > slot;
    std::vector<bool> onStack;
    int ints = 0, sses = 0;
    int stackSlots = 0;                 // eight-byte units going to memory
    for (const ExprPtr &arg : n.args()) {
        const Type *t = arg->type();
        std::vector<bool> lanes;
        // A struct of more than two eightbytes is MEMORY class outright. A
        // smaller one is classified, and then goes to memory anyway if its
        // eightbytes cannot all be placed - System V passes an aggregate whole
        // or not at all, never half in registers.
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

    // The stack must be sixteen-byte aligned at the call, and the arguments
    // that go on it are part of what sits there - so the padding is decided
    // with them counted, before any of it is pushed.
    int padSlots = ((depth_ + stackSlots) % 2 != 0) ? 1 : 0;
    if (padSlots) { out_ << "  sub $8, %rsp\n"; depth_++; }

    // The address to call is evaluated before the arguments and pushed beneath
    // them, so it comes back last - after every argument register has been
    // filled, by an instruction that cannot disturb one. %r11 holds it:
    // caller-saved, so the callee is free to destroy it, and not among the six
    // the System V argument sequence uses.
    // The memory arguments first, and in reverse: push moves downwards, so the
    // last one pushed ends up at the lowest address, and the callee reads them
    // upwards from 16(%rbp). Evaluating these before the register arguments is
    // allowed - C leaves the order of arguments unspecified, which is the same
    // licence that lets gcc go right to left.
    for (std::size_t i = n.args().size(); i-- > 0; ) {
        if (!onStack[i]) continue;
        const Type *t = n.args()[i]->type();
        n.args()[i]->accept(*this);
        if (!t->isStructOrUnion()) {
            if (t->isFloating()) pushF(); else push();
            continue;
        }
        // %rax is the struct's address. Its eightbytes go down in reverse for
        // the same reason the arguments do.
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
        // What was pushed for a struct is its address, so the eightbytes are
        // fetched from it. %rax holds the address and is not an argument
        // register, so nothing already filled is disturbed.
        pop("%rax");
        int size = t->size(target_);
        for (std::size_t k = 0; k < isSse[i].size(); k++) {
            int off = static_cast<int>(k) * 8;
            int left = size - off;
            // The last eightbyte of a struct that does not fill one is loaded
            // at its real width. Reading eight bytes would be reading past the
            // object, and the bits above it are unspecified anyway.
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
    if (n.callee() != nullptr) pop("%r11");

    bool pad = (depth_ % 2) != 0;
    if (pad) out_ << "  sub $8, %rsp\n";
    // %al carries the number of vector registers used. A variadic callee reads
    // it to know how much of its register save area to fill; get it wrong and
    // printf("%f") reads the wrong place. It was a constant zero until floating
    // point existed, which is why nothing noticed until now.
    out_ << "  mov $" << (n.isVariadic() ? sses : 0) << ", %rax\n";
    if (n.callee() != nullptr) out_ << "  call *%r11\n";
    else                       out_ << "  call " << n.name() << "\n";
    if (pad) out_ << "  add $8, %rsp\n";

    // The memory arguments and their padding come off together. They were the
    // callee's to read and are nobody's now.
    if (stackSlots + padSlots > 0) {
        out_ << "  add $" << (stackSlots + padSlots) * 8 << ", %rsp\n";
        depth_ -= stackSlots + padSlots;
    }

    // A struct comes back in up to two registers, and everything above expects
    // a struct to be an address - so it is written into the slot the parser
    // reserved and the slot's address is what the expression yields.
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

    // An integer result arrives in %eax with the high half undefined and has to
    // be put back into canonical form. A floating result is already in %xmm0.
    if (!n.type()->isVoid() && !n.type()->isFloating()) canonicalise(n.type());
}

// A condition is true when it differs from zero, and for a floating value that
// is a comparison against a zeroed register rather than a cmp against an
// immediate. Everything that branches goes through here so neither file is
// forgotten.
void X86_64Linux::genTruth(const Expr &e) {
    e.accept(*this);
    if (!e.type()->isFloating()) return;
    bool isDouble = e.type()->kind() == Kind::Double;
    out_ << "  pxor %xmm1, %xmm1\n";
    out_ << (isDouble ? "  ucomisd %xmm1, %xmm0\n" : "  ucomiss %xmm1, %xmm0\n");
    out_ << "  setne %al\n";
    out_ << "  movzbq %al, %rax\n";
}

// ---- statements ----

void X86_64Linux::visit(const ExprStmt &n) { n.expr().accept(*this); }

void X86_64Linux::visit(const Return &n) {
    n.value().accept(*this);

    // %rax holds the struct's address; the caller expects its eightbytes in
    // registers. The address is moved out of the way first, because %rax is
    // itself one of the two the value goes back in.
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

// continue goes to the step, not to the condition. That is the whole reason
// this is not lowered to a while: putting the step at the end of the body
// would let a continue skip it.
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

// The body runs before the condition is ever asked, which is the point of it.
void X86_64Linux::visit(const DoWhile &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    out_ << label("begin", id) << ":\n";
    n.body().accept(*this);
    out_ << label("step", id) << ":\n";     // continue re-tests, as C says
    genTruth(n.cond());
    out_ << "  cmp $0, %rax\n";
    out_ << "  jne " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";

    jumps_.pop_back();
}

// A chain of comparisons, then the body. Not a jump table: that wants the
// values sorted and the span weighed against the count, and it is worth doing
// when there is something to measure it against.
//
// The comparisons are emitted before the body, so every jump lands on a label
// somewhere inside it - including one nested in a block, which is legal C and
// works here for a reason worth stating: nothing about the frame depends on how
// control arrived. Every slot was allocated by the prologue, so jumping into
// the middle of a block skips no setup that anything later needs.
void X86_64Linux::visit(const Switch &n) {
    int id = nextLabel();

    // The parser converted the controlling expression to its promoted type and
    // every case value to that same type, so %rax and the constant are already
    // the same 64-bit pattern and the comparison asks nothing about width.
    n.cond().accept(*this);
    for (const Case *c : n.cases()) {
        long v = c->value();
        // cmp takes a 32-bit immediate and sign-extends it, so a value outside
        // that range has to be materialised first. Without this, "case
        // 4294967295" on an unsigned switch would compare against -1.
        if (v >= -2147483648L && v <= 2147483647L) {
            out_ << "  cmp $" << v << ", %rax\n";
        } else {
            out_ << "  movabs $" << v << ", %rdx\n";
            out_ << "  cmp %rdx, %rax\n";
        }
        out_ << "  je " << label("case", c->id()) << "\n";
    }
    // No case matched. A switch without a default falls out of the whole
    // statement, which is why this is a jump and not a fallthrough into the
    // body sitting immediately below it.
    out_ << "  jmp "
         << (n.defaultCase() ? label("default", n.defaultCase()->id())
                             : label("end", id))
         << "\n";

    jumps_.push_back({ label("end", id), "" });
    n.body().accept(*this);
    jumps_.pop_back();
    out_ << label("end", id) << ":\n";
}

// The label, then the statement it labels. Falling from one case into the next
// is not a special case here - it is what emitting the body in source order
// already does, and stopping is the thing that would need code.
void X86_64Linux::visit(const Case &n) {
    out_ << label(n.isDefault() ? "default" : "case", n.id()) << ":\n";
    n.body().accept(*this);
}

// The parser has already checked that the label exists somewhere in this
// function, so there is nothing to decide here - only a name to spell the same
// way it is spelled where it is defined.
void X86_64Linux::visit(const Goto &n) {
    out_ << "  jmp " << userLabel(n.label()) << "\n";
}

void X86_64Linux::visit(const Label &n) {
    out_ << userLabel(n.name()) << ":\n";
    n.body().accept(*this);
}

// Shaped like an If, and deliberately not written by reusing one: an If leaves
// no value, and the whole point of this is that both arms leave theirs in the
// same place. That place is decided by the result type the parser worked out,
// and both arms already carry it, so nothing here chooses a register.
//
// Only one arm runs. The jump over the second is what makes "p ? *p : 0" safe,
// which is the reason to have the operator rather than a function.
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

// The left operand is emitted for what it does, not for what it leaves: its
// value is overwritten by the right one, which is exactly what C says happens.
void X86_64Linux::visit(const Comma &n) {
    n.left().accept(*this);
    n.right().accept(*this);
}

void X86_64Linux::visit(const Break &) {
    out_ << "  jmp " << jumps_.back().brk << "\n";
}

// Past any switch in between, to the nearest enclosing loop. The parser has
// already refused a continue with no loop to reach, so the search always ends.
void X86_64Linux::visit(const Continue &) {
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            out_ << "  jmp " << jumps_[i].cont << "\n";
            return;
        }
    }
}

// ---- functions ----

// Closes the current buffer and keeps its text. Called after the data section
// and after each function, so chunks_ ends up in source order whatever order
// the functions were built in.
std::string X86_64Linux::label(const char *kind, int id) const {
    return labelPrefix_ + kind + "." + std::to_string(id);
}

// The "user." in the middle is not decoration. It keeps a label the programmer
// wrote in its own space, so a function containing "end:" cannot collide with
// the compiler's own end label for a loop in the same function.
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
    labels_ = 0;                      // safe now that labels carry the name
    labelPrefix_ = ".L." + fn.name() + ".";
    returnLabel_ = ".L.return." + fn.name();

    // static means internal linkage here as it does for an object: the symbol
    // is defined and simply not exported.
    if (!fn.isStatic()) out_ << "  .globl " << fn.name() << "\n";
    out_ << "  .text\n";
    out_ << fn.name() << ":\n";
    out_ << "  push %rbp\n";
    out_ << "  mov %rsp, %rbp\n";
    if (fn.frameSize() > 0) out_ << "  sub $" << fn.frameSize() << ", %rsp\n";

    // Each argument register is stored with the width of its own parameter: a
    // char parameter occupies one byte of the frame, not eight.
    const std::vector<Param> &ps = fn.params();
    int ints = 0, sses = 0;
    // Where the first memory argument sits: past the saved %rbp and the return
    // address the call pushed. The caller laid them out upwards from there in
    // the order they were written.
    int stackAt = 16;
    for (std::size_t i = 0; i < ps.size(); i++) {
        const Type *pt = ps[i].type;
        // The same question the caller asked, answered the same way, because
        // the two have to agree exactly: does this argument fit in what is left
        // of the registers, or did it go to memory?
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

        // A struct arrived in one or two registers and has to be reassembled in
        // the frame, because everything above here reaches a struct by address.
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
        // Written straight to the slot. Going through an address register
        // would have to be %rdi, and %rdi is itself an incoming argument -
        // spilling the first parameter destroyed the second, which showed up
        // as a recursive function that never reached its base case.
        storeAt(ps[i].type, ps[i].offset);
    }

    fn.body().accept(*this);

    // Falling off the end returns 0, which is what C promises for main. For a
    // function returning double the answer lives in %xmm0, so zeroing %rax
    // would say nothing - the register is zeroed instead.
    if (fn.returns()->isFloating()) out_ << "  pxor %xmm0, %xmm0\n";
    else                            out_ << "  mov $0, %rax\n";
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

            // The pieces arrive in offset order and cover only what the
            // initialiser named. Everything between them is padding, or an
            // element the list did not reach, and both are zero - which is what
            // C says an aggregate with a short initialiser holds.
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

    // The only place the destination is touched. Source order, not completion
    // order - which is what keeps the output identical from run to run, and
    // would keep it identical if these were produced concurrently.
    for (const std::string &chunk : chunks_) sink_ << chunk;
}
