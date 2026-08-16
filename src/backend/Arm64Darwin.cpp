#include "Arm64Darwin.h"

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
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int DarwinArm64Target::alignOf(Kind k) const { return sizeOf(k); }

// x8 carries the indirect return pointer, a register of its own - so unlike
// System V it does not push every other argument along by one.
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

// The stack stays sixteen-byte aligned at all times, which AArch64 requires of
// sp rather than merely preferring: one slot is 16 bytes even for 8 of payload.
void Arm64Darwin::push() { out_ << "  str x0, [sp, #-16]!\n"; }
void Arm64Darwin::pop(const char *reg) {
    out_ << "  ldr " << reg << ", [sp], #16\n";
}

// A float lives in the low half of its register and every write to the 's' view
// zeroes the top, so spilling the 'd' view is exact for both widths and there is
// one pair of these rather than two.
void Arm64Darwin::pushD() { out_ << "  str d0, [sp, #-16]!\n"; }
void Arm64Darwin::popD(const char *reg) {
    out_ << "  ldr " << reg << ", [sp], #16\n";
}

// Which view of a vector register a value of this type occupies: 's' for float,
// 'd' for double. They are the same register, and naming the wrong one is an
// instruction that assembles and computes at the wrong precision.
static std::string fpReg(const Type *t, int n) {
    return std::string(t->kind() == Kind::Float ? "s" : "d") + std::to_string(n);
}

void Arm64Darwin::movImm(const char *reg, long value) {
    unsigned long u = static_cast<unsigned long>(value);
    out_ << "  mov " << reg << ", #" << (u & 0xffff) << "\n";
    for (int shift = 16; shift < 64; shift += 16) {
        unsigned long part = (u >> shift) & 0xffff;
        if (part != 0) out_ << "  movk " << reg << ", #" << part
                            << ", lsl #" << shift << "\n";
    }
}

// AArch64 has no way to name an arbitrary floating constant in an instruction,
// so the bit pattern goes through an integer register. fmov between the two
// files is a move of the bits and not a conversion.
void Arm64Darwin::loadFpConst(const std::string &reg, const Type *t, double v) {
    if (t->kind() == Kind::Float) {
        float f = static_cast<float>(v);
        unsigned int bits;
        std::memcpy(&bits, &f, sizeof bits);
        movImm("x9", static_cast<long>(bits));
        out_ << "  fmov " << reg << ", w9\n";
    } else {
        unsigned long bits;
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

// The k'th eightbyte of an aggregate that is 'size' bytes long, narrowed on the
// last one so nothing is written past the object. A 12-byte struct travels in
// two registers but occupies twelve bytes, and storing the second in full would
// put four bytes into whatever the frame keeps next door.
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
            // Imported, and Mach-O has no copy relocation to pretend otherwise.
            // The symbol lives in some dylib at an address only the loader
            // knows, so what this image holds is a GOT slot containing it - one
            // load further than the local case, and the linker refuses the
            // local form outright rather than silently getting it wrong:
            // "fixup error (kind=arm64_adrp_lo12) ... does not have address".
            //
            // 'stdout' is the one every C programmer meets. ELF hides this
            // difference with a copy relocation, which is why the same
            // page-addressing serves for both there.
            out_ << "  adrp x0, _" << v->name() << "@GOTPAGE\n";
            out_ << "  ldr x0, [x0, _" << v->name() << "@GOTPAGEOFF]\n";
        }
        return;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') { u->operand().accept(*this); return; }
    }
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(&e)) {
        // A bit-field has no address in C, and this is the point that knows it.
        // The unit carrying it does, which is what bitFieldUnitAddr is for.
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
    // A struct-valued call or '?:' has already left its address in x0. Neither
    // is an lvalue and the parser refuses '&' on both, so this is only reached
    // by a member selection reading out of one.
    if (const Call *c = dynamic_cast<const Call *>(&e)) {
        if (c->type()->isStructOrUnion()) { c->accept(*this); return; }
    }
    if (const Conditional *q = dynamic_cast<const Conditional *>(&e)) {
        if (q->type()->isStructOrUnion()) { q->accept(*this); return; }
    }
    unsupported("the address of this expression");
}

// 'add' takes a 12-bit unsigned immediate and a member can sit further into a
// struct than that, so anything larger is materialised first. x9 is the
// scratch the rest of this backend already borrows.
void Arm64Darwin::addOffset(int bytes) {
    if (bytes == 0) return;
    if (bytes > 0 && bytes < 4096) {
        out_ << "  add x0, x0, #" << bytes << "\n";
        return;
    }
    movImm("x9", bytes);
    out_ << "  add x0, x0, x9\n";
}

// Struct assignment, which C defines as copying the bytes. Widest first, so a
// well-aligned struct moves eight at a time and only the tail is picked over.
// The offsets ride in the addressing mode, whose immediate is scaled by the
// access width and reaches far enough for any struct this compiler can build a
// frame for.
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

// Shift the field up to the top of the register and back down again: the way
// down is arithmetic for a signed field and logical for an unsigned one, which
// is the whole of the sign extension.
void Arm64Darwin::bitFieldExtract(const MemberAccess &m) {
    load(m.type());
    int left = 64 - m.bitOffset() - m.width();
    int right = 64 - m.width();
    if (left > 0) out_ << "  lsl x0, x0, #" << left << "\n";
    out_ << (m.type()->isSigned(target_) ? "  asr x0, x0, #" : "  lsr x0, x0, #")
         << right << "\n";
}

// x0 holds the value being assigned and x1 the address of the unit that
// carries the field. Read the unit, clear the field's bits, drop the new ones
// in, write it back - and leave in x0 the value the assignment itself has,
// which is the field as it now reads rather than what was handed in.
void Arm64Darwin::bitFieldInsert(const MemberAccess &m) {
    unsigned long ones = (m.width() == 64) ? ~0UL : ((1UL << m.width()) - 1);
    unsigned long mask = ones << m.bitOffset();

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

// Apple's second departure from AAPCS64, and the one with teeth. The standard
// gives every stack argument an eight-byte slot; Apple gives it its own size at
// its own alignment, so four ints past the registers occupy sixteen bytes and
// not thirty-two. Confirmed against clang rather than assumed - it stores them
// at [x9], [x9,#4], [x9,#8], [x9,#12], and a char, an int and a long land at
// 0, 4 and 8.
//
// The variadic rule is the *other* one: eight bytes each whatever the type,
// which is why both live in this file and why only the named part comes
// through here.
int Arm64Darwin::stackArgSlot(const Type *t, int &at) const {
    at = alignTo(at, t->align(target_));
    int here = at;
    at += t->size(target_);
    return here;
}

// An aggregate on the stack does not follow the packed rule a scalar does.
// Measured against clang: a 12-byte struct occupies sixteen bytes there, and
// one placed after a char starts at offset 8 rather than 4 - so the size is
// rounded up to a multiple of eight and the alignment is at least eight, even
// when the type's own is four.
//
// That makes three slot rules in this file, and they are genuinely three:
// packed for a named scalar, rounded for a named aggregate, and always eight
// for anything variadic.
int Arm64Darwin::aggStackSlot(const Type *t, const AggPlan &p, int &at) const {
    if (p.byRef) {
        // Only the pointer to the caller's copy travels here.
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

// Apple departs from AAPCS64 and puts the whole variadic part on the stack, in
// eight-byte slots, never in registers - which is the same departure the caller
// half of visit(Call) already makes. So there is no register save area to
// describe and no pair of offsets to track: the va_list is a pointer, and
// starting the walk means pointing it at the first slot.
//
// The caller leaves those slots at the stack pointer as the call is made, and
// this prologue is 'stp x29, x30, [sp, #-16]!' followed by 'mov x29, sp'. So
// the first variadic slot is sixteen bytes above x29, past the saved frame
// pointer and link register.
//
// A named parameter that did not fit in a register sits in front of the
// variadic part and moves it, so the walk starts past those too.
// namedStackBytes_ is what the prologue measured while laying them out - this
// used to be a hardcoded sixteen, correct only while such a parameter could
// not exist.
void Arm64Darwin::visit(const VaStart &n) {
    n.list().accept(*this);
    out_ << "  add x1, x29, #" << (16 + namedStackBytes_) << "\n";
    out_ << "  str x1, [x0]\n";
}

// Read the slot the walk is pointing at and step it. Every slot is eight bytes
// wide whatever the type, because the default argument promotions have already
// turned a float into a double and anything narrower than an int into one.
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
    // A bit-field is written through the unit that carries it, so the address
    // pushed is the unit's rather than the field's - the field has none.
    const MemberAccess *bf = dynamic_cast<const MemberAccess *>(&n.target());
    if (bf != nullptr && !bf->isBitField()) bf = nullptr;

    if (bf) bitFieldUnitAddr(*bf);
    else    genAddr(n.target());
    push();
    n.value().accept(*this);
    pop("x1");

    // Assigning a whole struct copies its bytes. The value side left the
    // source's address in x0 rather than a value, because load() declines to
    // load an aggregate into a register - there is no register that size.
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
        if (from->kind() != to->kind())
            out_ << "  fcvt " << fpReg(to, 0) << ", " << fpReg(from, 0) << "\n";
        return;
    }
    if (!fromF && toF) {
        // From x0 rather than w0 whatever the source width, because an integer
        // here is always already extended to 64 bits for its own type - so one
        // instruction serves char through long, and the signedness picks it.
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
        // Unordered leaves Z clear, so 'eq' is false and !NaN is 0 - which is
        // right, NaN being true. The x86 twin of this needed two flags.
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

        // The IEEE conditions, which are not the signed integer ones. After
        // fcmp an unordered result sets C and V with N and Z clear, so 'lt'
        // (N!=V) would call NaN < x true. 'mi' (N set) is the one that reads
        // false, and 'le' has the same problem where 'ls' does not. This is
        // exactly the trap the x86 backend fell into from the other direction.
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

    // Every one of these computes in the full 64-bit register, and the result
    // has to be cut back to the width of its own type before anything reads it.
    // 'int i = 100000; i * i' overflows at 32 bits and C says it wraps there;
    // left in x0 it is the 64-bit product, which compares equal to the long
    // multiplication it is supposed to differ from. The x86-64 backend calls
    // canonicalise at each of these sites for the same reason.
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

// Three things in flight: the address, the old value that is the expression's
// result, and the new value that gets stored. Built as a node rather than as
// '(x += 1) - 1' because that rewrite is wrong wherever the type wraps.
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

    // An indirect result travels in x8, a register of its own, so unlike
    // System V it does not push every other argument along by one.
    bool sret = n.type()->isStructOrUnion() &&
                planFor(n.type()).byRef;

    // The two register files are counted independently under AAPCS64, so a
    // named argument takes the next register of its own kind rather than the
    // one at its position. Worked out before anything is evaluated, because the
    // registers are filled in reverse afterwards.
    //
    // An aggregate is not one register but a plan: an HFA fills consecutive
    // vector registers, a small struct one or two integer ones, and a large one
    // a single integer register holding the address of the caller's copy.
    std::vector<std::string> dest;
    std::vector<AggPlan> plans(named);
    std::vector<int> firstReg(named, 0);
    // -1 while the argument fits in a register; an offset once its file is
    // spent and it has to travel in memory instead.
    std::vector<int> stackOff(named, -1);
    int ints = 0, floats = 0;
    int stackAt = 0;
    for (std::size_t i = 0; i < named; i++) {
        const Type *t = args[i]->type();
        if (t->isStructOrUnion()) {
            plans[i] = planFor(t);
            // An aggregate goes wholly in registers or wholly in memory; it is
            // never split across the two. And when it goes to memory it takes
            // the rest of its own register file with it - every later argument
            // of that kind goes to memory too, even though registers remain.
            //
            // Measured, not assumed: with seven integer registers spent and a
            // two-word struct following, clang puts the struct in memory and
            // then puts the *next* int in memory as well, leaving x7 unused.
            // The other file is untouched by this - a double after that struct
            // still arrives in d0.
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
        // The two files are counted independently, so one can be spent while
        // the other still has room - and only the spent one overflows.
        if (t->isFloating()) {
            if (floats < abi_.sseCount) { dest.push_back(fpReg(t, floats++)); continue; }
        } else {
            if (ints < abi_.intCount) { dest.push_back(abi_.intRegs[ints++]); continue; }
        }
        stackOff[i] = stackArgSlot(t, stackAt);
        dest.push_back("");
    }

    // Apple's deviation from AAPCS64: the variadic part goes on the stack in
    // eight-byte slots, never in registers. Follow the standard here and printf
    // reads whatever was lying in x0-x7. A float has already been promoted to
    // double by the default argument promotions, so every slot is eight wide.
    //
    // It begins after any *named* arguments that overflowed, because arguments
    // are laid out in the order they were written. The two use different slot
    // rules - packed for the named part, eight bytes for the variadic - and a
    // call can have both.
    int variadicBase = alignTo(stackAt, 8);
    int extraBytes = alignTo(variadicBase + static_cast<int>(extra) * 8, 16);
    if (extraBytes > 0) {
        movImm("x9", extraBytes);
        out_ << "  sub sp, sp, x9\n";
    }
    for (std::size_t i = 0; i < named; i++) {
        if (stackOff[i] < 0) continue;
        args[i]->accept(*this);
        storeToStack(args[i]->type(), stackOff[i]);
    }
    for (std::size_t k = 0; k < extra; k++) {
        const ExprPtr &a = args[named + k];
        a->accept(*this);
        if (a->type()->isFloating())
            out_ << "  str d0, [sp, #" << (variadicBase + k * 8) << "]\n";
        else
            out_ << "  str x0, [sp, #" << (variadicBase + k * 8) << "]\n";
    }

    // Where to branch to, when that is an expression rather than a name.
    // Evaluated once and kept on the stack while the arguments are marshalled:
    // it cannot sit in a register across that, because every argument register
    // is about to be written and an argument expression is free to use the
    // scratch ones - including, if it contains an indirect call of its own,
    // the very register this would use.
    //
    // It is pushed *after* the variadic area is opened rather than before, and
    // that order is load-bearing. Both live on the stack, and the pop below is
    // what puts sp back to the base of the variadic part - which is where the
    // callee expects to find it on entry. Pushed first, it would still be
    // underneath at the branch and every variadic argument would be eight
    // bytes further away than the callee looks.
    if (n.callee() != nullptr) {
        n.callee()->accept(*this);
        push();
    }

    // Aggregates first, each copied into the frame slot the parser reserved for
    // it. That slot is addressed off x29 and so cannot be disturbed by anything
    // evaluated afterwards, which is what keeps a multi-register argument out of
    // the push-and-pop dance below - and for a large one the copy is the thing
    // the ABI requires the caller to make anyway.
    for (std::size_t i = 0; i < named; i++) {
        if (!args[i]->type()->isStructOrUnion()) continue;
        args[i]->accept(*this);
        movImm("x9", n.argSlot(i));
        out_ << "  sub x1, x29, x9\n";
        copyBlock(args[i]->type()->size(target_), "x0", "x1");
    }

    // Only the ones with a register to be popped into. An aggregate is read
    // back out of its frame slot below, and one that went to memory has
    // already been stored - so dest being empty is the test for both, and it
    // cannot drift the way naming the two cases separately would.
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

    // Now the aggregates, read back out of their slots. x9 is scratch and is
    // not an argument register, so nothing placed above is disturbed.
    for (std::size_t i = 0; i < named; i++) {
        const Type *t = args[i]->type();
        if (!t->isStructOrUnion()) continue;
        movImm("x9", n.argSlot(i));
        out_ << "  sub x9, x29, x9\n";
        const AggPlan &p = plans[i];

        // The ones that travel in memory. x9 already holds the address of the
        // caller's copy, which is what a by-reference aggregate hands over and
        // what a by-value one is copied from. x11 for the destination, because
        // copyBlock carries the bytes in x10.
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
        // x16 is the intra-procedure-call scratch register, and this is what
        // AArch64 reserves it for: not an argument register, not preserved
        // across a call, and the documented place to put a branch target
        // computed at run time. Popping here is also what returns sp to the
        // base of the variadic part.
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
            // The callee wrote through x8 into that very slot.
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

    if (!n.type()->isVoid() && n.type()->size(target_) == 4 &&
        n.type()->isSigned(target_))
        out_ << "  sxtw x0, w0\n";
}

void Arm64Darwin::visit(const ExprStmt &n) { n.expr().accept(*this); }

void Arm64Darwin::visit(const Return &n) {
    if (!n.hasValue()) { out_ << "  b " << returnLabel_ << "\n"; return; }
    n.value().accept(*this);

    const Type *t = n.value().type();
    if (t->isStructOrUnion()) {
        // x0 holds the address of the value being returned.
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
            // Read the far word first: the near one lands in x0, which is the
            // register the address is still sitting in.
            static const char *const ret[2] = { "x0", "x1" };
            for (int k = p.words; k-- > 0; )
                out_ << "  ldr " << ret[k] << ", [x0, #" << (k * 8) << "]\n";
        }
    }
    out_ << "  b " << returnLabel_ << "\n";
}

void Arm64Darwin::visit(const Block &n) {
    for (const StmtPtr &s : n.body()) s->accept(*this);
}

void Arm64Darwin::visit(const If &n) {
    int id = nextLabel();
    genTruth(n.cond());
    out_ << "  cmp x0, #0\n";
    out_ << "  beq " << label("else", id) << "\n";
    n.thenArm().accept(*this);
    out_ << "  b " << label("end", id) << "\n";
    out_ << label("else", id) << ":\n";
    if (n.elseArm() != nullptr) n.elseArm()->accept(*this);
    out_ << label("end", id) << ":\n";
}

void Arm64Darwin::visit(const While &n) {
    int id = nextLabel();
    jumps_.push_back(JumpTargets{ label("end", id), label("begin", id) });
    out_ << label("begin", id) << ":\n";
    genTruth(n.cond());
    out_ << "  cmp x0, #0\n";
    out_ << "  beq " << label("end", id) << "\n";
    n.body().accept(*this);
    out_ << "  b " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";
    jumps_.pop_back();
}

void Arm64Darwin::visit(const DoWhile &n) {
    int id = nextLabel();
    jumps_.push_back(JumpTargets{ label("end", id), label("cont", id) });
    out_ << label("begin", id) << ":\n";
    n.body().accept(*this);
    out_ << label("cont", id) << ":\n";
    genTruth(n.cond());
    out_ << "  cmp x0, #0\n";
    out_ << "  bne " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";
    jumps_.pop_back();
}

void Arm64Darwin::visit(const For &n) {
    int id = nextLabel();
    jumps_.push_back(JumpTargets{ label("end", id), label("cont", id) });
    if (n.init() != nullptr) n.init()->accept(*this);
    out_ << label("begin", id) << ":\n";
    if (n.cond() != nullptr) {
        genTruth(*n.cond());
        out_ << "  cmp x0, #0\n";
        out_ << "  beq " << label("end", id) << "\n";
    }
    n.body().accept(*this);
    out_ << label("cont", id) << ":\n";
    if (n.step() != nullptr) n.step()->accept(*this);
    out_ << "  b " << label("begin", id) << "\n";
    out_ << label("end", id) << ":\n";
    jumps_.pop_back();
}

void Arm64Darwin::visit(const Break &) {
    out_ << "  b " << jumps_.back().brk << "\n";
}

// Past any switch between here and the loop. A switch pushes a break target
// with no continue target, so the innermost entry that names one is the loop
// this 'continue' belongs to - which is the rule C states and the reason the
// search is a loop rather than a look at the top of the stack.
void Arm64Darwin::visit(const Continue &) {
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            out_ << "  b " << jumps_[i].cont << "\n";
            return;
        }
    }
}

void Arm64Darwin::visit(const Conditional &n) {
    int id = nextLabel();
    genTruth(n.cond());
    out_ << "  cmp x0, #0\n";
    out_ << "  beq " << label("else", id) << "\n";
    n.thenArm().accept(*this);
    out_ << "  b " << label("end", id) << "\n";
    out_ << label("else", id) << ":\n";
    n.elseArm().accept(*this);
    out_ << label("end", id) << ":\n";
}

void Arm64Darwin::visit(const Comma &n) {
    n.left().accept(*this);
    n.right().accept(*this);
}

// The subject is evaluated once, into x0, and then compared against each case
// in turn. A jump table would be denser where the values are, but a chain is
// what the x86-64 backend does and the two are worth keeping legible against
// each other; nothing here depends on the cases being sorted or contiguous.
//
// The body is emitted as one statement, labels and all, so a 'case' reached by
// falling out of the statement above it needs no branch - which is what makes
// fallthrough, and Duff's device with it, come out right for free.
void Arm64Darwin::visit(const Switch &n) {
    int id = nextLabel();

    n.cond().accept(*this);
    for (const Case *c : n.cases()) {
        long v = c->value();
        // 'cmp' takes a 12-bit unsigned immediate. Everything else - every
        // negative value included - has to be materialised first, and x9 is
        // the scratch register the rest of this backend already borrows.
        //
        // The comparison is over the whole of x0 because an integer here is
        // always already extended to 64 bits for its own type: a negative int
        // case value sign-extends to the same pattern the subject holds, and
        // an unsigned one zero-extends to the same pattern in its turn.
        if (v >= 0 && v < 4096) {
            out_ << "  cmp x0, #" << v << "\n";
        } else {
            movImm("x9", v);
            out_ << "  cmp x0, x9\n";
        }
        out_ << "  beq " << label("case", c->id()) << "\n";
    }
    out_ << "  b "
         << (n.defaultCase() != nullptr ? label("default", n.defaultCase()->id())
                                        : label("end", id))
         << "\n";

    // A switch is a break target and not a continue target: 'continue' inside
    // one belongs to the enclosing loop. The empty string says so, and
    // visit(Continue) walks past it to find the loop.
    jumps_.push_back(JumpTargets{ label("end", id), "" });
    n.body().accept(*this);
    jumps_.pop_back();
    out_ << label("end", id) << ":\n";
}

void Arm64Darwin::visit(const Case &n) {
    out_ << label(n.isDefault() ? "default" : "case", n.id()) << ":\n";
    n.body().accept(*this);
}

void Arm64Darwin::visit(const Goto &n) {
    out_ << "  b " << userLabel(n.label()) << "\n";
}

void Arm64Darwin::visit(const Label &n) {
    out_ << userLabel(n.name()) << ":\n";
    n.body().accept(*this);
}

// Mach-O aligns by a power of two where ELF aligns by a byte count, so the
// same answer from the type model is spelled differently in the two files.
static int p2AlignOf(int bytes) {
    int p = 0;
    while ((1 << p) < bytes) p++;
    return p;
}

void Arm64Darwin::emitGlobal(const Global &g, Segment seg) {
    int size = g.type->size(target_);
    int p2 = p2AlignOf(g.type->align(target_));
    if (!g.isStatic) out_ << "  .globl _" << g.name << "\n";

    // .zerofill takes the segment, the section, the symbol, its size and its
    // alignment, and defines the symbol itself - so unlike every other case
    // here there is no label to write and no bytes to follow.
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

        // An address constant. Mach-O prefixes a C symbol with an underscore -
        // but a string literal's label is not a C symbol, it is one the
        // compiler invented, and it is defined here without the underscore. So
        // the prefix goes on names and not on labels, which are the ones
        // beginning with a dot.
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
    // Narrow and wide literals go to different sections, and the reason is not
    // tidiness. '__cstring,cstring_literals' tells the linker its contents are
    // NUL-terminated C strings that it may split apart and deduplicate against
    // other units - which is exactly wrong for a wide literal, where the NULs
    // are *inside* the characters. L"hi" put there is cut at its first zero
    // byte, coalesced with some unrelated string, and reads as rubbish. So a
    // wide literal goes to __TEXT,__const, which has no such meaning.
    //
    // '.ascii' and not '.asciz' throughout: the terminator is already in the
    // bytes, and .asciz would append another.
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

    // Same three buckets as the ELF backend, in the same order, decided by the
    // same function. Only the spelling is Mach-O's - and .bss has no section
    // directive at all here, because .zerofill carries its own.
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
    labels_ = 0;
    functionName_ = fn.name();
    labelPrefix_ = "L." + fn.name() + ".";
    returnLabel_ = "L.return." + fn.name();


    out_ << "  .section __TEXT,__text,regular,pure_instructions\n";
    if (!fn.isStatic()) out_ << "  .globl _" << fn.name() << "\n";
    out_ << "  .p2align 2\n";
    out_ << "_" << fn.name() << ":\n";
    out_ << "  stp x29, x30, [sp, #-16]!\n";
    out_ << "  mov x29, sp\n";

    int frame = alignTo(fn.frameSize(), 16);
    if (frame > 0) {
        movImm("x9", frame);
        out_ << "  sub sp, sp, x9\n";
    }

    // An indirect result arrives in x8 and has to be kept somewhere x8 itself
    // will not survive to: it is call-clobbered, and this function may call.
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
            // The same question the caller asked, answered by the same
            // functions in the same order. An aggregate arrives wholly in
            // registers or wholly in memory, and one that arrived in memory
            // closed its own register file to everything after it.
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
                // It is sitting in the caller's stack area. A by-reference one
                // left a pointer there and the object is still the caller's,
                // so take a copy the way the register path does; a by-value one
                // left its bytes, which are already ours to copy from.
                out_ << "  mov x11, x9\n";
                out_ << "  add x9, x29, #" << (16 + from) << "\n";
                if (p.byRef) out_ << "  ldr x9, [x9]\n";
                copyBlock(ps[i].type->size(target_), "x9", "x11");
                continue;
            }

            if (p.byRef) {
                // A pointer to the caller's copy. Taking our own keeps every
                // later mention of the parameter an ordinary local.
                //
                // x11 for the destination, and not x1: the source is whichever
                // argument register this parameter arrived in, and for the
                // second parameter that register *is* x1 - which a destination
                // parked there would destroy before a byte had moved. x10 is
                // out too, being what copyBlock carries the bytes in.
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
            // It arrived in the caller's stack area, which begins sixteen
            // bytes above x29 - past the frame pointer and link register this
            // prologue pushed. Move it into this parameter's own frame slot, so
            // every later mention of it is an ordinary local and nothing below
            // has to know where it came from.
            //
            // The cursor is the same one the caller ran, by the same function.
            // That is the whole point: these two walks disagreeing by a single
            // byte produces a program that runs and returns nonsense.
            //
            // Copied as bytes, through x9/x10/x11, rather than loaded into x0
            // and stored from there. The parameters are walked in order and the
            // ones in registers are still sitting in them - so touching x0 here
            // destroys a *later* parameter that arrived in it. That is not
            // hypothetical: it is what this did, and a function taking a stack
            // argument followed by an integer in x0 read garbage for the
            // second. A frame slot is exactly the parameter's size, and so is
            // its incoming slot, which is what makes the byte copy the whole
            // of the job.
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

    // What va_start needs when a function has named parameters on the stack as
    // well as a variadic part: the variadic slots begin after them.
    namedStackBytes_ = alignTo(stackAt, 8);

    fn.body().accept(*this);

    out_ << "  mov x0, #0\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov sp, x29\n";
    out_ << "  ldp x29, x30, [sp], #16\n";
    out_ << "  ret\n";
}

void Arm64Darwin::run(const Program &program) {
    definedHere_.clear();
    for (const Global &g : program.globals)   definedHere_.insert(g.name);
    for (const Function &f : program.functions) definedHere_.insert(f.name());

    emitData(program);
    for (const Function &fn : program.functions) emitFunction(fn);
    out_ << ".subsections_via_symbols\n";
    sink_ << out_.str();
}
