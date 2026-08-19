#include "Arm64Darwin.h"

#include "../Source.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>

int DarwinArm64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                       return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 8;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;
    // Apple's arm64 makes long double another name for double.
    case Kind::LongDouble:                                 return 8;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int DarwinArm64Target::alignOf(Kind k) const { return sizeOf(k); }

// x8 carries the indirect return pointer, a register of its own.
static const char *const kArgRegs[] = { "x0", "x1", "x2", "x3",
                                        "x4", "x5", "x6", "x7" };
static const char *const kSseRegs[] = { "d0", "d1", "d2", "d3",
                                        "d4", "d5", "d6", "d7" };

static const Abi kAapcs64AppleAbi = {
    kArgRegs, 8,
    kSseRegs, 8,
    false,   // the two files are counted independently, as under System V
    0,       // no shadow space
    16,      // a struct over 16 bytes comes back through the pointer in x8
    true,    // an oversized aggregate is passed by reference
    false,   // no %al convention; and Apple puts variadic arguments on the stack
    "x9", "w9",
    true,    // an HFA travels in vector registers, whatever its size
    false,   // Mach-O, and this backend writes its own directives anyway
};

const Abi &Arm64DarwinBackend::abi() const { return kAapcs64AppleAbi; }

static const char *const kDarwinMacros[] = {
    "__aarch64__=1", "__arm64__=1", "__arm64=1",
    "__APPLE__=1", "__MACH__=1", "__LP64__=1", "_LP64=1", nullptr,
};
const char *const *Arm64DarwinBackend::identityMacros() const { return kDarwinMacros; }

std::unique_ptr<CodeGen> Arm64DarwinBackend::codegen(std::ostream &sink) const {
    return std::unique_ptr<CodeGen>(new Arm64Darwin(sink, target_, kAapcs64AppleAbi));
}

static int alignTo(int n, int a) { return (n + a - 1) / a * a; }

void Arm64Darwin::unsupported(const char *what) {
    std::fprintf(stderr, "codegen: %s is not supported yet by the arm64-darwin "
                         "backend\n", what);
    std::exit(1);
}

std::string Arm64Darwin::label(const char *kind, int id) const {
    return labelPrefix_ + kind + "." + std::to_string(id);
}

std::string Arm64Darwin::userLabel(const std::string &name) const {
    return "L." + functionName_ + ".user." + name;
}

// The stack stays sixteen-byte aligned at all times, which AArch64 requires.
void Arm64Darwin::push() { out_ << "  str x0, [sp, #-16]!\n"; }
void Arm64Darwin::pop(const char *reg) {
    out_ << "  ldr " << reg << ", [sp], #16\n";
}

// A float lives in the low half of its register and every write to the 's'
// view zeroes the top, so spills go through 'd' - exact for both.
void Arm64Darwin::pushD() { out_ << "  str d0, [sp, #-16]!\n"; }
void Arm64Darwin::popD(const char *reg) {
    out_ << "  ldr " << reg << ", [sp], #16\n";
}

// Asked of the type at every site: naming the wrong view assembles cleanly and
// computes at the wrong precision.
static std::string fpReg(const Type *t, int n) {
    return std::string(t->kind() == Kind::Float ? "s" : "d") + std::to_string(n);
}

void Arm64Darwin::movImm(const char *reg, long long value) {
    unsigned long long u = static_cast<unsigned long long>(value);
    out_ << "  mov " << reg << ", #" << (u & 0xffff) << "\n";
    for (int shift = 16; shift < 64; shift += 16) {
        unsigned long long part = (u >> shift) & 0xffff;
        if (part != 0) out_ << "  movk " << reg << ", #" << part
                            << ", lsl #" << shift << "\n";
    }
}

// AArch64 cannot name an arbitrary floating constant in an instruction, so it
// goes through an integer register.
void Arm64Darwin::loadFpConst(const std::string &reg, const Type *t, double v) {
    if (t->kind() == Kind::Float) {
        float f = static_cast<float>(v);
        unsigned int bits;
        std::memcpy(&bits, &f, sizeof bits);
        movImm("x9", static_cast<long>(bits));
        out_ << "  fmov " << reg << ", w9\n";
    } else {
        unsigned long long bits;
        std::memcpy(&bits, &v, sizeof bits);
        movImm("x9", static_cast<long>(bits));
        out_ << "  fmov " << reg << ", x9\n";
    }
}

void Arm64Darwin::narrowInt(const Type *to) {
    int sz = to->size(target_);
    bool sign = to->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  sxtb x0, w0\n" : "  uxtb w0, w0\n");
    else if (sz == 2) out_ << (sign ? "  sxth x0, w0\n" : "  uxth w0, w0\n");
    else if (sz == 4) out_ << (sign ? "  sxtw x0, w0\n" : "  mov w0, w0\n");
}

Arm64Darwin::AggPlan Arm64Darwin::planFor(const Type *t) const {
    AggPlan p;
    Kind elem;
    int n = homogeneousFloatCount(t, &elem);
    if (n > 0) { p.hfa = n; p.elem = elem; return p; }
    int size = t->size(target_);
    if (size <= 16) { p.words = (size + 7) / 8; return p; }
    p.byRef = true;
    p.words = 1;
    return p;
}

// The k'th eightbyte, narrowed on the last one so a 12-byte struct does not
// write four bytes into the neighbouring slot.
void Arm64Darwin::storeWord(const char *xreg, const char *base, int k, int size) {
    int off = k * 8;
    int left = size - off;
    std::string w = std::string("w") + (xreg + 1);
    if (left >= 8)      out_ << "  str "  << xreg << ", [" << base << ", #" << off << "]\n";
    else if (left >= 4) out_ << "  str "  << w    << ", [" << base << ", #" << off << "]\n";
    else if (left >= 2) out_ << "  strh " << w    << ", [" << base << ", #" << off << "]\n";
    else                out_ << "  strb " << w    << ", [" << base << ", #" << off << "]\n";
}

void Arm64Darwin::genAddr(const Expr &e) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        if (v->isLocal()) {
            // Computed rather than folded: a frame can outgrow the offset field.
            movImm("x9", v->offset());
            out_ << "  sub x0, x29, x9\n";
        } else if (definedHere_.count(v->name()) != 0) {
            out_ << "  adrp x0, _" << v->name() << "@PAGE\n";
            out_ << "  add x0, x0, _" << v->name() << "@PAGEOFF\n";
        } else {
            // Darwin reaches an imported symbol through the GOT - @GOTPAGE and
            // @GOTPAGEOFF, never @PAGE. Mach-O has no copy relocation to hide this the
            // way ELF does.
            out_ << "  adrp x0, _" << v->name() << "@GOTPAGE\n";
            out_ << "  ldr x0, [x0, _" << v->name() << "@GOTPAGEOFF]\n";
        }
        return;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') { u->operand().accept(*this); return; }
    }
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(&e)) {
        if (m->isBitField()) {
            std::fprintf(stderr,
                         "codegen: '%s' is a bit-field and has no address\n",
                         m->name().c_str());
            std::exit(1);
        }
        genAddr(m->object());
        addOffset(m->offset());
        return;
    }
    if (const StrLit *s = dynamic_cast<const StrLit *>(&e)) {
        out_ << "  adrp x0, " << s->label() << "@PAGE\n";
        out_ << "  add x0, x0, " << s->label() << "@PAGEOFF\n";
        return;
    }
    if (const Call *c = dynamic_cast<const Call *>(&e)) {
        if (c->type()->isStructOrUnion()) { c->accept(*this); return; }
    }
    if (const Conditional *q = dynamic_cast<const Conditional *>(&e)) {
        if (q->type()->isStructOrUnion()) { q->accept(*this); return; }
    }
    unsupported("the address of this expression");
}

// 'add' takes a 12-bit unsigned immediate and a member can sit further in.
void Arm64Darwin::addOffset(int bytes) {
    if (bytes == 0) return;
    if (bytes > 0 && bytes < 4096) {
        out_ << "  add x0, x0, #" << bytes << "\n";
        return;
    }
    movImm("x9", bytes);
    out_ << "  add x0, x0, x9\n";
}

// Widest first, so a trailing remainder is copied once.
void Arm64Darwin::copyBlock(int size, const char *from, const char *to) {
    int off = 0;
    while (size - off >= 8) {
        out_ << "  ldr x10, [" << from << ", #" << off << "]\n";
        out_ << "  str x10, [" << to << ", #" << off << "]\n";
        off += 8;
    }
    while (size - off >= 4) {
        out_ << "  ldr w10, [" << from << ", #" << off << "]\n";
        out_ << "  str w10, [" << to << ", #" << off << "]\n";
        off += 4;
    }
    while (size - off >= 2) {
        out_ << "  ldrh w10, [" << from << ", #" << off << "]\n";
        out_ << "  strh w10, [" << to << ", #" << off << "]\n";
        off += 2;
    }
    while (size - off >= 1) {
        out_ << "  ldrb w10, [" << from << ", #" << off << "]\n";
        out_ << "  strb w10, [" << to << ", #" << off << "]\n";
        off += 1;
    }
}

void Arm64Darwin::bitFieldUnitAddr(const MemberAccess &m) {
    genAddr(m.object());
    addOffset(m.offset());
}

// Shift the field to the top of the register and back down: an arithmetic
// shift sign-extends a signed field, a logical one zero-fills.
void Arm64Darwin::bitFieldExtract(const MemberAccess &m) {
    load(m.type());
    int left = 64 - m.bitOffset() - m.width();
    int right = 64 - m.width();
    if (left > 0) out_ << "  lsl x0, x0, #" << left << "\n";
    out_ << (m.type()->isSigned(target_) ? "  asr x0, x0, #" : "  lsr x0, x0, #")
         << right << "\n";
}

void Arm64Darwin::bitFieldInsert(const MemberAccess &m) {
    unsigned long long ones = (m.width() == 64) ? ~0ULL : ((1ULL << m.width()) - 1);
    unsigned long long mask = ones << m.bitOffset();

    movImm("x10", static_cast<long>(ones));
    out_ << "  and x10, x0, x10\n";
    if (m.bitOffset() != 0)
        out_ << "  lsl x10, x10, #" << m.bitOffset() << "\n";

    out_ << "  mov x11, x0\n";          // the value, kept for the result
    out_ << "  mov x0, x1\n";
    load(m.type());
    movImm("x9", static_cast<long>(~mask));
    out_ << "  and x0, x0, x9\n";
    out_ << "  orr x0, x0, x10\n";
    storeThrough(m.type(), "x1");

    out_ << "  mov x0, x11\n";
    int right = 64 - m.width();
    out_ << "  lsl x0, x0, #" << right << "\n";
    out_ << (m.type()->isSigned(target_) ? "  asr x0, x0, #" : "  lsr x0, x0, #")
         << right << "\n";
}

void Arm64Darwin::load(const Type *t) {
    if (t->isArray() || t->isStructOrUnion()) return;
    if (t->isFloating()) {
        out_ << "  ldr " << fpReg(t, 0) << ", [x0]\n";
        return;
    }

    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  ldrsb x0, [x0]\n" : "  ldrb w0, [x0]\n");
    else if (sz == 2) out_ << (sign ? "  ldrsh x0, [x0]\n" : "  ldrh w0, [x0]\n");
    else if (sz == 4) out_ << (sign ? "  ldrsw x0, [x0]\n" : "  ldr w0, [x0]\n");
    else              out_ << "  ldr x0, [x0]\n";
}

// Apple's departure from AAPCS64, and the one with teeth: a *named* stack
// argument is packed to its own size and alignment where the standard gives
// every one eight bytes. Read off clang rather than assumed.
int Arm64Darwin::stackArgSlot(const Type *t, int &at) const {
    at = alignTo(at, t->align(target_));
    int here = at;
    at += t->size(target_);
    return here;
}

// An aggregate on the stack does not follow the packed rule a scalar does: at
// least 8, rounded up to a multiple of 8.
int Arm64Darwin::aggStackSlot(const Type *t, const AggPlan &p, int &at) const {
    if (p.byRef) {
        at = alignTo(at, 8);
        int here = at;
        at += 8;
        return here;
    }
    int a = t->align(target_);
    if (a < 8) a = 8;
    at = alignTo(at, a);
    int here = at;
    at += alignTo(t->size(target_), 8);
    return here;
}

void Arm64Darwin::storeToStack(const Type *t, int off) {
    if (t->isFloating()) {
        out_ << "  str " << fpReg(t, 0) << ", [sp, #" << off << "]\n";
        return;
    }
    switch (t->size(target_)) {
    case 1:  out_ << "  strb w0, [sp, #" << off << "]\n"; return;
    case 2:  out_ << "  strh w0, [sp, #" << off << "]\n"; return;
    case 4:  out_ << "  str w0, [sp, #" << off << "]\n"; return;
    default: out_ << "  str x0, [sp, #" << off << "]\n"; return;
    }
}

void Arm64Darwin::storeThrough(const Type *t, const char *addrReg) {
    if (t->isFloating()) {
        out_ << "  str " << fpReg(t, 0) << ", [" << addrReg << "]\n";
        return;
    }
    switch (t->size(target_)) {
    case 1:  out_ << "  strb w0, [" << addrReg << "]\n"; return;
    case 2:  out_ << "  strh w0, [" << addrReg << "]\n"; return;
    case 4:  out_ << "  str w0, [" << addrReg << "]\n"; return;
    default: out_ << "  str x0, [" << addrReg << "]\n"; return;
    }
}

void Arm64Darwin::visit(const Num &n) {
    if (n.type()->isFloating()) {
        loadFpConst(fpReg(n.type(), 0), n.type(), n.dvalue());
        return;
    }
    movImm("x0", n.value());
}

void Arm64Darwin::visit(const Var &n) { genAddr(n); load(n.type()); }

// Apple puts the whole variadic part on the stack, eight bytes each, where
// AAPCS64 would use the register files. So printf's arguments are on the stack
// and a va_list is one pointer.
void Arm64Darwin::visit(const VaStart &n) {
    n.list().accept(*this);
    out_ << "  add x1, x29, #" << (16 + namedStackBytes_) << "\n";
    out_ << "  str x1, [x0]\n";
}

// Every slot is eight bytes whatever the type.
void Arm64Darwin::visit(const VaArg &n) {
    n.list().accept(*this);
    out_ << "  ldr x1, [x0]\n";
    out_ << "  add x2, x1, #8\n";
    out_ << "  str x2, [x0]\n";
    out_ << "  mov x0, x1\n";
    load(n.type());
}

void Arm64Darwin::visit(const StrLit &n) { genAddr(n); }

void Arm64Darwin::visit(const MemberAccess &n) {
    if (n.isBitField()) {
        bitFieldUnitAddr(n);
        bitFieldExtract(n);
        return;
    }
    genAddr(n);
    load(n.type());
}

void Arm64Darwin::visit(const Assign &n) {
    const MemberAccess *bf = dynamic_cast<const MemberAccess *>(&n.target());
    if (bf != nullptr && !bf->isBitField()) bf = nullptr;

    // The value goes first and the address second - the setjmp reason given in the
    // x86-64 backend applies here unchanged.
    n.value().accept(*this);
    bool inFp = n.type()->isFloating();
    if (inFp) pushD(); else push();

    if (bf) bitFieldUnitAddr(*bf);
    else    genAddr(n.target());
    out_ << "  mov x1, x0\n";

    if (inFp) popD("d0"); else pop("x0");

    if (n.type()->isStructOrUnion()) {
        copyBlock(n.type()->size(target_), "x0", "x1");
        out_ << "  mov x0, x1\n";
        return;
    }
    if (bf) { bitFieldInsert(*bf); return; }

    storeThrough(n.type(), "x1");
}

void Arm64Darwin::visit(const Cast &n) {
    n.value().accept(*this);
    genConversion(n.value().type(), n.type());
}

void Arm64Darwin::genConversion(const Type *from, const Type *to) {
    if (to->isVoid()) return;

    bool fromF = from->isFloating(), toF = to->isFloating();

    if (fromF && toF) {
        // The registers rather than the kinds, because two kinds can name one register
        // and the width is what decides the instruction.
        if (fpReg(to, 0) != fpReg(from, 0))
            out_ << "  fcvt " << fpReg(to, 0) << ", " << fpReg(from, 0) << "\n";
        return;
    }
    if (!fromF && toF) {
        // From x0 rather than w0 whatever the source width, because an integer has
        // already been widened to its own type in the full register.
        out_ << (from->isSigned(target_) ? "  scvtf " : "  ucvtf ")
             << fpReg(to, 0) << ", x0\n";
        return;
    }
    if (fromF && !toF) {
        out_ << (to->isSigned(target_) ? "  fcvtzs x0, " : "  fcvtzu x0, ")
             << fpReg(from, 0) << "\n";
        narrowInt(to);
        return;
    }
    narrowInt(to);
}

void Arm64Darwin::genTruth(const Expr &e) {
    e.accept(*this);
    if (e.type()->isFloating()) {
        out_ << "  fcmp " << fpReg(e.type(), 0) << ", #0.0\n";
        out_ << "  cset x0, ne\n";
        return;
    }
    out_ << "  cmp x0, #0\n";
    out_ << "  cset x0, ne\n";
}

void Arm64Darwin::visit(const Unary &n) {
    switch (n.op()) {
    case '-':
        n.operand().accept(*this);
        if (n.type()->isFloating()) {
            std::string r = fpReg(n.type(), 0);
            out_ << "  fneg " << r << ", " << r << "\n";
        } else {
            out_ << "  neg x0, x0\n";
        }
        return;
    case '~':
        n.operand().accept(*this);
        out_ << "  mvn x0, x0\n";
        return;
    case '!':
        n.operand().accept(*this);
        // Unordered leaves Z clear, so 'eq' is false and !NaN is 0.
        if (n.operand().type()->isFloating())
            out_ << "  fcmp " << fpReg(n.operand().type(), 0) << ", #0.0\n";
        else
            out_ << "  cmp x0, #0\n";
        out_ << "  cset x0, eq\n";
        return;
    case '&':
        genAddr(n.operand());
        return;
    case '*':
        n.operand().accept(*this);
        load(n.type());
        return;
    default:
        unsupported("this unary operator");
    }
}

void Arm64Darwin::visit(const Binary &n) {
    if (n.op() == BinOp::LAnd || n.op() == BinOp::LOr) {
        int id = nextLabel();
        bool isAnd = n.op() == BinOp::LAnd;
        genTruth(n.lhs());
        out_ << "  cmp x0, #0\n";
        out_ << (isAnd ? "  beq " : "  bne ") << label("shortcut", id) << "\n";
        genTruth(n.rhs());
        out_ << label("shortcut", id) << ":\n";
        return;
    }

    if (n.lhs().type()->isFloating() || n.rhs().type()->isFloating()) {
        const Type *ft = n.lhs().type();
        std::string a = fpReg(ft, 0), b = fpReg(ft, 1);

        n.lhs().accept(*this);
        pushD();
        n.rhs().accept(*this);
        out_ << "  fmov d1, d0\n";
        popD("d0");

        switch (n.op()) {
        case BinOp::Add: out_ << "  fadd " << a << ", " << a << ", " << b << "\n"; return;
        case BinOp::Sub: out_ << "  fsub " << a << ", " << a << ", " << b << "\n"; return;
        case BinOp::Mul: out_ << "  fmul " << a << ", " << a << ", " << b << "\n"; return;
        case BinOp::Div: out_ << "  fdiv " << a << ", " << a << ", " << b << "\n"; return;
        default: break;
        }

        // The IEEE conditions, which are not the signed integer ones: 'mi' and 'ls'
        // rather than 'lt' and 'le', because unordered must answer false.
        const char *cond = nullptr;
        switch (n.op()) {
        case BinOp::Eq: cond = "eq"; break;
        case BinOp::Ne: cond = "ne"; break;
        case BinOp::Lt: cond = "mi"; break;
        case BinOp::Le: cond = "ls"; break;
        case BinOp::Gt: cond = "gt"; break;
        case BinOp::Ge: cond = "ge"; break;
        default: unsupported("this operator on floating point");
        }
        out_ << "  fcmp " << a << ", " << b << "\n";
        out_ << "  cset x0, " << cond << "\n";
        return;
    }

    n.lhs().accept(*this);
    push();
    n.rhs().accept(*this);
    out_ << "  mov x1, x0\n";
    pop("x0");

    bool sign = n.lhs().type()->isSigned(target_);

    // Every one of these computes in the full 64-bit register, so the result is
    // cut back to its own type's width - without which 'int i=100000; i*i' does
    // not wrap. The x86-64 backend calls the same thing canonicalise.
    switch (n.op()) {
    case BinOp::Add: out_ << "  add x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::Sub: out_ << "  sub x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::Mul: out_ << "  mul x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::Div:
        out_ << (sign ? "  sdiv x0, x0, x1\n" : "  udiv x0, x0, x1\n");
        narrowInt(n.type());
        return;
    case BinOp::Mod:
        out_ << (sign ? "  sdiv x2, x0, x1\n" : "  udiv x2, x0, x1\n");
        out_ << "  msub x0, x2, x1, x0\n";
        narrowInt(n.type());
        return;
    case BinOp::BitAnd: out_ << "  and x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::BitOr:  out_ << "  orr x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::BitXor: out_ << "  eor x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::Shl:    out_ << "  lsl x0, x0, x1\n"; narrowInt(n.type()); return;
    case BinOp::Shr:
        out_ << (sign ? "  asr x0, x0, x1\n" : "  lsr x0, x0, x1\n");
        narrowInt(n.type());
        return;
    default: break;
    }

    const char *cond = nullptr;
    switch (n.op()) {
    case BinOp::Eq: cond = "eq"; break;
    case BinOp::Ne: cond = "ne"; break;
    case BinOp::Lt: cond = sign ? "lt" : "lo"; break;
    case BinOp::Le: cond = sign ? "le" : "ls"; break;
    case BinOp::Gt: cond = sign ? "gt" : "hi"; break;
    case BinOp::Ge: cond = sign ? "ge" : "hs"; break;
    default: unsupported("this binary operator");
    }
    out_ << "  cmp x0, x1\n";
    out_ << "  cset x0, " << cond << "\n";
}

void Arm64Darwin::visit(const Postfix &n) {
    genAddr(n.target());
    push();
    load(n.type());

    if (n.type()->isFloating()) {
        std::string a = fpReg(n.type(), 0), b = fpReg(n.type(), 1);
        pushD();
        loadFpConst(b, n.type(), 1.0);
        out_ << (n.increment() ? "  fadd " : "  fsub ")
             << a << ", " << a << ", " << b << "\n";
        popD("d1");
        pop("x2");
        storeThrough(n.type(), "x2");
        out_ << "  fmov d0, d1\n";
        return;
    }

    push();
    movImm("x9", n.step());
    out_ << (n.increment() ? "  add x0, x0, x9\n" : "  sub x0, x0, x9\n");
    pop("x1");
    pop("x2");
    storeThrough(n.type(), "x2");
    out_ << "  mov x0, x1\n";
}

void Arm64Darwin::visit(const Call &n) {
    const std::vector<ExprPtr> &args = n.args();
    std::size_t named = static_cast<std::size_t>(n.namedArgs());
    if (named > args.size()) named = args.size();
    std::size_t extra = args.size() - named;

    // An indirect result travels in x8, a register of its own.
    bool sret = n.type()->isStructOrUnion() &&
                planFor(n.type()).byRef;

    // The two register files are counted independently under AAPCS64, so a call
    // can run out of integer registers while vector ones remain.
    std::vector<std::string> dest;
    std::vector<AggPlan> plans(named);
    std::vector<int> firstReg(named, 0);
    std::vector<int> stackOff(named, -1);
    int ints = 0, floats = 0;
    int stackAt = 0;
    for (std::size_t i = 0; i < named; i++) {
        const Type *t = args[i]->type();
        if (t->isStructOrUnion()) {
            plans[i] = planFor(t);
            // An aggregate goes wholly in registers or wholly in memory, never split - and
            // one that goes to memory closes its own file to every later argument while
            // leaving the other open. A homogeneous float aggregate travels in that many
            // vector registers whatever its size.
            if (plans[i].hfa > 0) {
                if (floats + plans[i].hfa <= abi_.sseCount) {
                    firstReg[i] = floats;
                    floats += plans[i].hfa;
                } else {
                    stackOff[i] = aggStackSlot(t, plans[i], stackAt);
                    floats = abi_.sseCount;
                }
            } else {
                if (ints + plans[i].words <= abi_.intCount) {
                    firstReg[i] = ints;
                    ints += plans[i].words;
                } else {
                    stackOff[i] = aggStackSlot(t, plans[i], stackAt);
                    ints = abi_.intCount;
                }
            }
            dest.push_back("");
            continue;
        }
        if (t->isFloating()) {
            if (floats < abi_.sseCount) { dest.push_back(fpReg(t, floats++)); continue; }
        } else {
            if (ints < abi_.intCount) { dest.push_back(abi_.intRegs[ints++]); continue; }
        }
        stackOff[i] = stackArgSlot(t, stackAt);
        dest.push_back("");
    }

    // Apple's deviation again: the variadic part is on the stack, so the named
    // arguments alone spend the register files.
    int variadicBase = alignTo(stackAt, 8);
    int extraBytes = alignTo(variadicBase + static_cast<int>(extra) * 8, 16);
    if (extraBytes > 0) {
        movImm("x9", extraBytes);
        out_ << "  sub sp, sp, x9\n";
    }
    for (std::size_t i = 0; i < named; i++) {
        if (stackOff[i] < 0) continue;
        // Not aggregates: the loop below evaluates those into their frame
        // slots, and running the expression here as well ran its side effects
        // twice - and stored the struct's *address* into its stack slot.
        if (args[i]->type()->isStructOrUnion()) continue;
        args[i]->accept(*this);
        storeToStack(args[i]->type(), stackOff[i]);
    }
    for (std::size_t k = 0; k < extra; k++) {
        const ExprPtr &a = args[named + k];
        // Refused rather than compiled wrong: an aggregate here left x0 - the
        // struct's address - in the slot, and the callee read a pointer where
        // C promises bytes. Valid C90, so this is a named gap in the style of
        // the others this backend declares, until the copy is written.
        if (a->type()->isStructOrUnion())
            unsupported("a struct passed through '...'");
        a->accept(*this);
        if (a->type()->isFloating())
            out_ << "  str d0, [sp, #" << (variadicBase + k * 8) << "]\n";
        else
            out_ << "  str x0, [sp, #" << (variadicBase + k * 8) << "]\n";
    }

    // Where to branch to, when that is an expression rather than a name.
    if (n.callee() != nullptr) {
        n.callee()->accept(*this);
        push();
    }

    // Aggregates first, each copied into the frame slot the parser reserved, so
    // the registers are free for everything else.
    for (std::size_t i = 0; i < named; i++) {
        if (!args[i]->type()->isStructOrUnion()) continue;
        args[i]->accept(*this);
        movImm("x9", n.argSlot(i));
        out_ << "  sub x1, x29, x9\n";
        copyBlock(args[i]->type()->size(target_), "x0", "x1");
    }

    for (std::size_t i = 0; i < named; i++) {
        if (dest[i].empty()) continue;
        args[i]->accept(*this);
        if (args[i]->type()->isFloating()) pushD(); else push();
    }
    for (std::size_t i = named; i-- > 0; ) {
        if (dest[i].empty()) continue;
        if (args[i]->type()->isFloating()) popD(dest[i].c_str());
        else                               pop(dest[i].c_str());
    }

    // x9 is scratch and is not one of the argument registers.
    for (std::size_t i = 0; i < named; i++) {
        const Type *t = args[i]->type();
        if (!t->isStructOrUnion()) continue;
        movImm("x9", n.argSlot(i));
        out_ << "  sub x9, x29, x9\n";
        const AggPlan &p = plans[i];

        if (stackOff[i] >= 0) {
            if (p.byRef) {
                out_ << "  str x9, [sp, #" << stackOff[i] << "]\n";
            } else {
                out_ << "  add x11, sp, #" << stackOff[i] << "\n";
                copyBlock(args[i]->type()->size(target_), "x9", "x11");
            }
            continue;
        }

        if (p.byRef) {
            out_ << "  mov " << abi_.intRegs[firstReg[i]] << ", x9\n";
        } else if (p.hfa > 0) {
            const char *w = (p.elem == Kind::Float) ? "s" : "d";
            int step = (p.elem == Kind::Float) ? 4 : 8;
            for (int k = 0; k < p.hfa; k++)
                out_ << "  ldr " << w << (firstReg[i] + k)
                     << ", [x9, #" << (k * step) << "]\n";
        } else {
            for (int k = 0; k < p.words; k++)
                out_ << "  ldr " << abi_.intRegs[firstReg[i] + k]
                     << ", [x9, #" << (k * 8) << "]\n";
        }
    }

    // x8 last, after every argument register is settled.
    if (sret) {
        movImm("x9", n.resultSlot());
        out_ << "  sub x8, x29, x9\n";
    }

    if (n.callee() != nullptr) {
        // x16 is the intra-procedure-call scratch register, which is what it is for.
        // A branch through any argument register would destroy an argument.
        pop("x16");
        out_ << "  blr x16\n";
    } else {
        out_ << "  bl _" << n.name() << "\n";
    }
    if (extraBytes > 0) {
        movImm("x9", extraBytes);
        out_ << "  add sp, sp, x9\n";
    }
    if (n.type()->isStructOrUnion()) {
        movImm("x9", n.resultSlot());
        out_ << "  sub x9, x29, x9\n";
        AggPlan p = planFor(n.type());
        if (p.byRef) {
        } else if (p.hfa > 0) {
            const char *w = (p.elem == Kind::Float) ? "s" : "d";
            int step = (p.elem == Kind::Float) ? 4 : 8;
            for (int k = 0; k < p.hfa; k++)
                out_ << "  str " << w << k << ", [x9, #" << (k * step) << "]\n";
        } else {
            static const char *const ret[2] = { "x0", "x1" };
            for (int k = 0; k < p.words; k++)
                storeWord(ret[k], "x9", k, n.type()->size(target_));
        }
        out_ << "  mov x0, x9\n";
        return;
    }

    // Every narrow integer return, not only the signed 4-byte one: the upper
    // bits of x0 after a call are the callee's leavings, and a clang-built
    // callee does not promise to clear them. The x86 backend canonicalises
    // every return for the same reason; the two rules now match.
    if (!n.type()->isVoid() && !n.type()->isFloating())
        narrowInt(n.type());
}

// The five spellings the shared statement walk asks of this target.
void Arm64Darwin::defineLabel(const std::string &l) { out_ << l << ":\n"; }
void Arm64Darwin::jump(const std::string &l) { out_ << "  b " << l << "\n"; }
void Arm64Darwin::branchIfZero(const std::string &l) {
    out_ << "  cmp x0, #0\n";
    out_ << "  beq " << l << "\n";
}
void Arm64Darwin::branchIfNotZero(const std::string &l) {
    out_ << "  cmp x0, #0\n";
    out_ << "  bne " << l << "\n";
}
// 'cmp' takes a 12-bit unsigned immediate; everything else goes through a
// register.
void Arm64Darwin::caseBranch(long long v, const std::string &l) {
    if (v >= 0 && v < 4096) {
        out_ << "  cmp x0, #" << v << "\n";
    } else {
        movImm("x9", v);
        out_ << "  cmp x0, x9\n";
    }
    out_ << "  beq " << l << "\n";
}

void Arm64Darwin::visit(const Return &n) {
    markLine(n);
    if (!n.hasValue()) { out_ << "  b " << returnLabel_ << "\n"; return; }
    n.value().accept(*this);

    const Type *t = n.value().type();
    if (t->isStructOrUnion()) {
        AggPlan p = planFor(t);
        if (p.byRef) {
            movImm("x9", sretSlot_);
            out_ << "  sub x9, x29, x9\n";
            out_ << "  ldr x1, [x9]\n";        // where the caller wants it
            copyBlock(t->size(target_), "x0", "x1");
            out_ << "  mov x0, x1\n";
        } else if (p.hfa > 0) {
            const char *w = (p.elem == Kind::Float) ? "s" : "d";
            int step = (p.elem == Kind::Float) ? 4 : 8;
            for (int k = 0; k < p.hfa; k++)
                out_ << "  ldr " << w << k << ", [x0, #" << (k * step) << "]\n";
        } else {
            // Read the far word first: the near one lands in x0, which is the register
            // holding the address.
            static const char *const ret[2] = { "x0", "x1" };
            for (int k = p.words; k-- > 0; )
                out_ << "  ldr " << ret[k] << ", [x0, #" << (k * 8) << "]\n";
        }
    }
    out_ << "  b " << returnLabel_ << "\n";
}

// Past any switch between here and the loop: a switch pushes a break target
// with no continue target of its own.
// The subject is evaluated once, into x0, then compared against each case.
// Mach-O aligns by a power of two where ELF aligns by a byte count.
static int p2AlignOf(int bytes) {
    int p = 0;
    while ((1 << p) < bytes) p++;
    return p;
}

void Arm64Darwin::emitGlobal(const Global &g, Segment seg) {
    int size = g.type->size(target_);
    int p2 = p2AlignOf(objectAlign(g.type, target_));
    if (!g.isStatic) out_ << "  .globl _" << g.name << "\n";

    // .zerofill takes the segment, the section, the symbol, its size and its
    // alignment - all five, and in that order.
    if (seg == Segment::Bss) {
        out_ << "  .zerofill __DATA,__bss,_" << g.name << ","
             << size << "," << p2 << "\n";
        return;
    }

    out_ << "  .p2align " << p2 << "\n";
    out_ << "_" << g.name << ":\n";
    int at = 0;
    for (const GlobalPiece &p : g.init) {
        if (p.offset > at) out_ << "  .space " << (p.offset - at) << "\n";

        // Mach-O prefixes a C symbol with an underscore, and an address constant names
        // the symbol as the assembler knows it.
        if (!p.symbol.empty()) {
            out_ << "  .quad " << (p.symbol[0] == '.' ? "" : "_") << p.symbol;
            if (p.value > 0) out_ << "+" << p.value;
            else if (p.value < 0) out_ << "-" << -p.value;
            out_ << "\n";
            at = p.offset + p.size;
            continue;
        }

        switch (p.size) {
        case 1:  out_ << "  .byte " << p.value << "\n"; break;
        case 2:  out_ << "  .short " << p.value << "\n"; break;
        case 4:  out_ << "  .long " << p.value << "\n"; break;
        default: out_ << "  .quad " << p.value << "\n"; break;
        }
        at = p.offset + p.size;
    }
    if (at < size) out_ << "  .space " << (size - at) << "\n";
}

void Arm64Darwin::emitData(const Program &program) {
    // Narrow and wide literals go to different sections: __cstring is for
    // NUL-terminated bytes, and a wide literal's four-byte elements would be cut
    // at the first zero byte.
    for (int pass = 0; pass < 2; pass++) {
        bool wantWide = (pass == 1);
        bool opened = false;
        for (const StringLit &s : program.strings) {
            if ((s.width > 1) != wantWide) continue;
            if (!opened) {
                out_ << (wantWide ? "  .section __TEXT,__const\n"
                                  : "  .section __TEXT,__cstring,cstring_literals\n");
                opened = true;
            }
            if (s.width > 1) {
                int p2 = 0;
                while ((1 << p2) < s.width) p2++;
                out_ << "  .p2align " << p2 << "\n";
            }
            out_ << s.label << ":\n";
            out_ << "  .ascii \"";
            for (unsigned char c : s.bytes) {
                if (c == '"' || c == '\\') out_ << '\\' << c;
                else if (c == '\n')        out_ << "\\n";
                else if (c == '\t')        out_ << "\\t";
                else if (c >= 32 && c < 127) out_ << c;
                else out_ << '\\' << static_cast<char>('0' + ((c >> 6) & 7))
                          << static_cast<char>('0' + ((c >> 3) & 7))
                          << static_cast<char>('0' + (c & 7));
            }
            out_ << "\"\n";
        }
    }

    struct Bucket { Segment seg; const char *open; };
    const Bucket order[] = {
        { Segment::Const, "  .section __TEXT,__const\n" },
        { Segment::Data,  "  .section __DATA,__data\n" },
        { Segment::Bss,   nullptr },
    };
    for (const Bucket &b : order) {
        bool opened = false;
        for (const Global &g : program.globals) {
            if (segmentFor(g) != b.seg) continue;
            if (!opened && b.open != nullptr) { out_ << b.open; }
            opened = true;
            emitGlobal(g, b.seg);
        }
    }
}

void Arm64Darwin::emitFunction(const Function &fn) {
    resetLabels();
    functionName_ = fn.name();
    labelPrefix_ = "L." + fn.name() + ".";
    returnLabel_ = "L.return." + fn.name();


    out_ << "  .section __TEXT,__text,regular,pure_instructions\n";
    if (!fn.isStatic()) out_ << "  .globl _" << fn.name() << "\n";
    out_ << "  .p2align 2\n";
    out_ << "_" << fn.name() << ":\n";
    if (const Source *src = lineSource()) {
        Source::Place at = src->locate(fn.pos());
        DwarfFunction d;
        d.name = fn.name();
        d.begin = "Lfunc.begin." + fn.name();
        d.end = "Lfunc.end." + fn.name();
        d.file = at.file + 1;
        d.line = at.line;
        d.external = !fn.isStatic();
        d.returns = fn.returns();
        d.locals = &fn.locals();
        dwarfFns_.push_back(d);
        resetBlocks(fn.blocks());
        out_ << d.begin << ":\n";
    }
    // The prologue belongs to the line the function was declared on, which is
    // where a debugger asked to stop on the name puts its breakpoint.
    markLine(fn.pos());
    out_ << "  stp x29, x30, [sp, #-16]!\n";
    out_ << "  mov x29, sp\n";

    int frame = alignTo(fn.frameSize(), 16);
    if (frame > 0) {
        movImm("x9", frame);
        out_ << "  sub sp, sp, x9\n";
    }

    // An indirect result arrives in x8 and has to be kept somewhere x8 itself is
    // not needed.
    sretSlot_ = fn.sretSlot();
    if (sretSlot_ != 0) {
        movImm("x9", sretSlot_);
        out_ << "  sub x9, x29, x9\n";
        out_ << "  str x8, [x9]\n";
    }

    const std::vector<Param> &ps = fn.params();
    int ints = 0, floats = 0;
    int stackAt = 0;
    for (std::size_t i = 0; i < ps.size(); i++) {
        if (ps[i].type->isStructOrUnion()) {
            AggPlan p = planFor(ps[i].type);
            // The same question the caller asked, answered by the same rule.
            bool inRegister = p.hfa > 0 ? floats + p.hfa <= abi_.sseCount
                                        : ints + p.words <= abi_.intCount;
            int from = inRegister ? -1 : aggStackSlot(ps[i].type, p, stackAt);
            if (!inRegister) {
                if (p.hfa > 0) floats = abi_.sseCount;
                else           ints = abi_.intCount;
            }

            movImm("x9", ps[i].offset);
            out_ << "  sub x9, x29, x9\n";

            if (!inRegister) {
                out_ << "  mov x11, x9\n";
                out_ << "  add x9, x29, #" << (16 + from) << "\n";
                if (p.byRef) out_ << "  ldr x9, [x9]\n";
                copyBlock(ps[i].type->size(target_), "x9", "x11");
                continue;
            }

            if (p.byRef) {
                // A pointer to the caller's copy: taking our own keeps every later mention an
                // ordinary local, and the callee may write through the pointer it was given.
                out_ << "  mov x11, x9\n";
                copyBlock(ps[i].type->size(target_), abi_.intRegs[ints++], "x11");
            } else if (p.hfa > 0) {
                const char *w = (p.elem == Kind::Float) ? "s" : "d";
                int step = (p.elem == Kind::Float) ? 4 : 8;
                for (int k = 0; k < p.hfa; k++)
                    out_ << "  str " << w << (floats + k)
                         << ", [x9, #" << (k * step) << "]\n";
                floats += p.hfa;
            } else {
                for (int k = 0; k < p.words; k++)
                    storeWord(abi_.intRegs[ints + k], "x9", k,
                              ps[i].type->size(target_));
                ints += p.words;
            }
            continue;
        }
        bool inRegister = ps[i].type->isFloating() ? floats < abi_.sseCount
                                                   : ints < abi_.intCount;
        movImm("x9", ps[i].offset);
        out_ << "  sub x9, x29, x9\n";

        if (!inRegister) {
            // It arrived in the caller's stack area, which begins sixteen bytes above the
            // frame pointer - the saved x29 and x30. Apple's packed rule decides the
            // offsets, so this walk repeats the caller's sum.
            int off = stackArgSlot(ps[i].type, stackAt);
            out_ << "  mov x11, x9\n";
            out_ << "  add x9, x29, #" << (16 + off) << "\n";
            copyBlock(ps[i].type->size(target_), "x9", "x11");
            continue;
        }

        if (ps[i].type->isFloating()) {
            out_ << "  str " << fpReg(ps[i].type, floats++) << ", [x9]\n";
        } else {
            out_ << "  mov x0, " << abi_.intRegs[ints++] << "\n";
            storeThrough(ps[i].type, "x9");
        }
    }

    // What va_start needs when named parameters are on the stack as well.
    namedStackBytes_ = alignTo(stackAt, 8);

    fn.body().accept(*this);

    out_ << "  mov x0, #0\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov sp, x29\n";
    out_ << "  ldp x29, x30, [sp], #16\n";
    out_ << "  ret\n";
    if (lineSource()) {
        out_ << "Lfunc.end." << fn.name() << ":\n";
        // Now rather than when the function was recorded: the labels bounding
        // its blocks are only known once its body has been walked.
        dwarfFns_.back().blocks = blocks();
    }
}

void Arm64Darwin::emitLoc(int file, int line, int column) {
    out_ << "  .loc " << file << " " << line << " " << column << "\n";
}

void Arm64Darwin::run(const Program &program) {
    definedHere_.clear();
    for (const Global &g : program.globals)   definedHere_.insert(g.name);
    for (const Function &f : program.functions) definedHere_.insert(f.name());

    // Named before anything refers to them: the assembler wants the file
    // declared before the first .loc that points at it.
    if (const Source *src = lineSource()) {
        const std::vector<std::string> &names = src->files();
        for (std::size_t i = 0; i < names.size(); i++)
            out_ << "  .file " << (i + 1) << " \"" << names[i] << "\"\n";
    }

    emitData(program);
    for (const Function &fn : program.functions) emitFunction(fn);
    if (const Source *src = lineSource()) {
        for (const Global &g : program.globals) {
            DwarfGlobal dg;
            dg.name = g.name;
            dg.symbol = g.name;
            dg.type = g.type;
            dg.external = !g.isStatic;
            dwarfGlobals_.push_back(dg);
        }
        std::string dwarf;
        writeDwarf(dwarf, kMachODwarf, target_, src->files().front(), compDir(),
                   dwarfFns_, dwarfGlobals_);
        out_ << dwarf;
    }
    out_ << ".subsections_via_symbols\n";
    sink_ << out_.str();
}
