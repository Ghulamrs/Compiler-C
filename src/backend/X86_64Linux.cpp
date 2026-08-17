#include "X86_64Linux.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

int LinuxX86_64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                   return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 8;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;
    // x87's 80-bit extended format occupies ten bytes and System V gives it
    // sixteen, so that an array of them keeps every element sixteen-aligned.
    // The six bytes of padding are not part of the value and are why 'long
    // double' cannot be compared by memcmp.
    case Kind::LongDouble:                                 return 16;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int LinuxX86_64Target::alignOf(Kind k) const { return sizeOf(k); }

static void classifyInto(const Type *t, int base, std::vector<bool> &sse,
                         const Target &target) {
    if (t->isStructOrUnion()) {
        for (const Member &m : t->members()) classifyInto(m.type, base + m.offset, sse, target);
        return;
    }
    if (t->isArray()) {
        int step = t->pointee()->size(target);
        for (long long i = 0; i < t->length(); i++)
            classifyInto(t->pointee(), base + static_cast<int>(i) * step, sse, target);
        return;
    }
    if (t->isFloating()) return;

    int from = base / 8;
    int to = (base + t->size(target) - 1) / 8;
    for (int i = from; i <= to && i < static_cast<int>(sse.size()); i++) sse[i] = false;
}

std::vector<bool> classifyEightbytes(const Type *t, const Target &target) {
    int size = t->size(target);
    std::vector<bool> sse(static_cast<std::size_t>((size + 7) / 8), true);
    classifyInto(t, 0, sse, target);
    return sse;
}

static const char *const kArgRegs[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3",
                                        "%xmm4", "%xmm5", "%xmm6", "%xmm7" };

static const Abi kSysVAbi = {
    kArgRegs, 6,
    kSseRegs, 8,
    false,   // the two register files are counted independently
    0,       // no shadow space
    16,      // a struct of 16 bytes or less comes back in registers
    false,   // an oversized aggregate is copied onto the stack, not referenced
    true,    // %al carries the SSE count for a variadic callee
    "%rdi", "%edi",  // call-clobbered here, so free between statements
    false,
    true,    // ELF: .type and .size mean something here
};

const Abi &X86_64LinuxBackend::abi() const { return kSysVAbi; }

static const char *const kLinuxMacros[] = {
    "__x86_64__=1", "__x86_64=1", "__amd64__=1", "__amd64=1",
    "__linux__=1", "__linux=1", "__unix__=1", "__unix=1",
    "__ELF__=1", "__LP64__=1", "_LP64=1", nullptr,
};
const char *const *X86_64LinuxBackend::identityMacros() const { return kLinuxMacros; }

void X86_64Linux::push() { a_->ins("push", reg("%rax")); depth_++; }
void X86_64Linux::pop(const char *into) { a_->ins("pop", reg(into)); depth_--; }

void X86_64Linux::pushF() {
    a_->ins("sub", immText("8"), reg("%rsp"));
    a_->ins("movsd", reg("%xmm0"), mem("%rsp"));
    depth_++;
}
void X86_64Linux::popF(const char *into) {
    a_->ins("movsd", mem("%rsp"), reg(into));
    a_->ins("add", immText("8"), reg("%rsp"));
    depth_--;
}

// x87 is a stack of eight registers, not a file this generator can allocate
// out of, so the only safe number of live values in it between one expression
// and the next is zero. Every long double therefore round-trips through memory
// exactly where an SSE value would - 'fstpt' stores ten bytes and pops, 'fldt'
// loads and pushes - and the sixteen taken here keep %rsp sixteen-aligned,
// which is also the two slots System V would give the value as an argument.
void X86_64Linux::pushX87() {
    a_->ins("sub", immText("16"), reg("%rsp"));
    a_->ins("fstpt", mem("%rsp"));
    depth_ += 2;
}
void X86_64Linux::popX87() {
    a_->ins("fldt", mem("%rsp"));
    a_->ins("add", immText("16"), reg("%rsp"));
    depth_ -= 2;
}

Kind X86_64Linux::genKind(const Type *t) const {
    if (t->kind() == Kind::LongDouble && !isX87(t)) return Kind::Double;
    return t->kind();
}

static int alignTo(int n, int a) { return (n + a - 1) / a * a; }

const char *X86_64Linux::acc(const Type *t) const {
    return t->size(target_) == 8 ? "%rax" : "%eax";
}
const char *X86_64Linux::rhs(const Type *t) const {
    return t->size(target_) == 8 ? abi_.scratch : abi_.scratch32;
}

// Microsoft x64: an aggregate travels in a register only when its size is
// exactly 1, 2, 4 or 8 bytes - the sizes a register can hold whole. Every other
// size, 3 and 5 and 6 and 7 as much as anything over 8, is copied by the caller
// and passed as a pointer to that copy. System V instead cuts an aggregate into
// eightbytes and classifies each, which is what classifyEightbytes above is
// for; the two conventions disagree about this more than about anything else.
static bool msInRegister(int size) {
    return size == 1 || size == 2 || size == 4 || size == 8;
}

// The narrower name of one of the two integer return registers: "%rax" at four
// bytes is "%eax", at two "%ax", at one "%al".
//
// A lookup rather than an index into the name, which is how this was wrong for
// a long time. It used to ask whether r[1] was 'a' - and r[1] is 'r' in both
// "%rax" and "%rdx", so the test was never true and every value came back in
// the %d-something. The two sides of a struct return were wrong in the same
// way, so cc1 agreed with itself and the differential suite could not see it:
// both compilers' programs printed the right answer, each internally
// consistent. It took building for a target whose *other* return path was
// right - Microsoft x64, where the caller reads %eax - for the two halves to
// disagree and the bug to become visible.
//
// The real cost was never the wrong register. It was that a struct returned by
// cc1 could not be read by a function gcc compiled, on either target.
static const char *narrower(const char *reg64, int bytes) {
    bool isA = std::strcmp(reg64, "%rax") == 0;
    if (bytes >= 4) return isA ? "%eax" : "%edx";
    if (bytes >= 2) return isA ? "%ax"  : "%dx";
    return isA ? "%al" : "%dl";
}


// %rax holds the address of the aggregate; leave in %rax what the ABI actually
// sends. Only %rax and %r11 are touched, because in the loop that fills the
// argument registers the ones after this are already live - and %rcx is the
// fourth of them under System V and the *first* under Windows.
void X86_64Linux::msAggregateToRax(const Type *t, int slot) {
    int size = t->size(target_);
    if (msInRegister(size)) {
        if (size == 8)      a_->ins("mov", mem("%rax"), reg("%rax"));
        else if (size == 4) a_->ins("movl", mem("%rax"), reg("%eax"));
        else if (size == 2) a_->ins("movzwl", mem("%rax"), reg("%eax"));
        else                a_->ins("movzbl", mem("%rax"), reg("%eax"));
        return;
    }
    msCopyToSlot(t, slot, "%rax");
    a_->ins("lea", mem(-(slot), "%rbp"), reg("%rax"));
}

// The caller's copy. The callee is entitled to write through the pointer it is
// given, so handing it the original object would let it modify ours.
void X86_64Linux::msCopyToSlot(const Type *t, int slot, const char *from) {
    int size = t->size(target_);
    int off = 0;
    while (size - off >= 8) {
        a_->ins("mov", mem(off, from), reg("%r11"));
        a_->ins("mov", reg("%r11"), mem((off - slot), "%rbp"));
        off += 8;
    }
    while (size - off >= 4) {
        a_->ins("movl", mem(off, from), reg("%r11d"));
        a_->ins("movl", reg("%r11d"), mem((off - slot), "%rbp"));
        off += 4;
    }
    while (size - off >= 2) {
        a_->ins("movzwl", mem(off, from), reg("%r11d"));
        a_->ins("movw", reg("%r11w"), mem((off - slot), "%rbp"));
        off += 2;
    }
    while (size - off >= 1) {
        a_->ins("movzbl", mem(off, from), reg("%r11d"));
        a_->ins("movb", reg("%r11b"), mem((off - slot), "%rbp"));
        off += 1;
    }
}

void X86_64Linux::unsupported(const char *what) {
    std::fprintf(stderr, "codegen: %s is not supported yet by the %s backend\n",
                 what, target_.name());
    std::exit(1);
}

// Which register an argument takes, and what taking it spends. System V counts
// the two files independently, so a call can run out of integer registers while
// SSE ones remain; Microsoft x64 numbers slots, so the third argument is %r8 or
// %xmm2 by its position and spending either file spends both.
int X86_64Linux::takeSlot(bool sse, int &ints, int &sses) const {
    int taken = sse ? sses : ints;
    if (abi_.positional) { ints++; sses++; }
    else if (sse)        sses++;
    else                 ints++;
    return taken;
}

void X86_64Linux::canonicalise(const Type *t) {
    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1) a_->ins(sign ? "movsbq" : "movzbq", reg("%al"), reg("%rax"));
    else if (sz == 2) a_->ins(sign ? "movswq" : "movzwq", reg("%ax"), reg("%rax"));
    else if (sz == 4) { if (sign) a_->ins("movslq", reg("%eax"), reg("%rax")); else a_->ins("mov", reg("%eax"), reg("%eax")); }
}

void X86_64Linux::genAddr(const Expr &e) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        if (v->isLocal()) a_->ins("lea", mem(-(v->offset()), "%rbp"), reg("%rax"));
        else              a_->ins("lea", rip(v->name()), reg("%rax"));
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
        if (m->offset() != 0) a_->ins("add", imm(m->offset()), reg("%rax"));
        return;
    }
    if (const StrLit *s = dynamic_cast<const StrLit *>(&e)) {
        a_->ins("lea", rip(s->label()), reg("%rax"));
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

    if (isX87(t))                    { a_->ins("fldt", mem("%rax")); return; }
    if (genKind(t) == Kind::Float)   { a_->ins("movss", mem("%rax"), reg("%xmm0")); return; }
    if (genKind(t) == Kind::Double)  { a_->ins("movsd", mem("%rax"), reg("%xmm0")); return; }

    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      a_->ins(sign ? "movsbq" : "movzbq", mem("%rax"), reg("%rax"));
    else if (sz == 2) a_->ins(sign ? "movswq" : "movzwq", mem("%rax"), reg("%rax"));
    else if (sz == 4) if (sign) a_->ins("movslq", mem("%rax"), reg("%rax")); else a_->ins("movl", mem("%rax"), reg("%eax"));
    else              a_->ins("movq", mem("%rax"), reg("%rax"));
}

void X86_64Linux::store(const Type *t) {
    const char *at = abi_.scratch;
    if (isX87(t))                   { a_->ins("fstpt", mem(at)); return; }
    if (genKind(t) == Kind::Float)  { a_->ins("movss", reg("%xmm0"), mem(at)); return; }
    if (genKind(t) == Kind::Double) { a_->ins("movsd", reg("%xmm0"), mem(at)); return; }

    switch (t->size(target_)) {
    case 1: a_->ins("movb", reg("%al"), mem(at)); return;
    case 2: a_->ins("movw", reg("%ax"), mem(at)); return;
    case 4: a_->ins("movl", reg("%eax"), mem(at)); return;
    default: a_->ins("movq", reg("%rax"), mem(at)); return;
    }
}

void X86_64Linux::storeAt(const Type *t, int offset) {
    if (isX87(t))                   { a_->ins("fstpt", mem(-(offset), "%rbp")); return; }
    if (genKind(t) == Kind::Float)  { a_->ins("movss", reg("%xmm0"), mem(-(offset), "%rbp")); return; }
    if (genKind(t) == Kind::Double) { a_->ins("movsd", reg("%xmm0"), mem(-(offset), "%rbp")); return; }
    switch (t->size(target_)) {
    case 1: a_->ins("movb", reg("%al"), mem(-(offset), "%rbp")); return;
    case 2: a_->ins("movw", reg("%ax"), mem(-(offset), "%rbp")); return;
    case 4: a_->ins("movl", reg("%eax"), mem(-(offset), "%rbp")); return;
    default: a_->ins("movq", reg("%rax"), mem(-(offset), "%rbp")); return;
    }
}

// The ten bytes of an 80-bit constant, built on the stack and loaded from
// there. A rodata entry would do as well, but this needs no label, no pool and
// no second pass - and the sixteen bytes are given back immediately.
void X86_64Linux::loadX87Const(long double v) {
    unsigned long long lo = 0;
    unsigned int hi = 0;
    x87Parts(v, &lo, &hi);

    a_->ins("sub", immText("16"), reg("%rsp"));
    a_->ins("movabs", imm(lo), reg("%rax"));
    a_->ins("mov", reg("%rax"), mem("%rsp"));
    a_->ins("movw", imm(hi), mem(8, "%rsp"));
    a_->ins("fldt", mem("%rsp"));
    a_->ins("add", immText("16"), reg("%rsp"));
}

void X86_64Linux::visit(const Num &n) {
    if (!n.type()->isFloating()) {
        a_->ins("mov", imm(n.value()), reg("%rax"));
        return;
    }
    if (isX87(n.type())) { loadX87Const(n.dvalue()); return; }
    if (genKind(n.type()) == Kind::Float) {
        float f = static_cast<float>(n.dvalue());
        unsigned int bits;
        std::memcpy(&bits, &f, 4);
        a_->ins("mov", imm(bits), reg("%eax"));
        a_->ins("movd", reg("%eax"), reg("%xmm0"));
    } else {
        double d = n.dvalue();
        unsigned long long bits;
        std::memcpy(&bits, &d, 8);
        a_->ins("movabs", imm(bits), reg("%rax"));
        a_->ins("movq", reg("%rax"), reg("%xmm0"));
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
    const char *to = abi_.scratch;
    int off = 0;
    while (size - off >= 8) {
        a_->ins("mov", mem(off, "%rax"), reg("%rcx"));
        a_->ins("mov", reg("%rcx"), mem(off, to));
        off += 8;
    }
    while (size - off >= 4) {
        a_->ins("movl", mem(off, "%rax"), reg("%ecx"));
        a_->ins("movl", reg("%ecx"), mem(off, to));
        off += 4;
    }
    while (size - off >= 2) {
        a_->ins("movw", mem(off, "%rax"), reg("%cx"));
        a_->ins("movw", reg("%cx"), mem(off, to));
        off += 2;
    }
    while (size - off >= 1) {
        a_->ins("movb", mem(off, "%rax"), reg("%cl"));
        a_->ins("movb", reg("%cl"), mem(off, to));
        off += 1;
    }
}

void X86_64Linux::bitFieldUnitAddr(const MemberAccess &m) {
    genAddr(m.object());
    if (m.offset() != 0) a_->ins("add", imm(m.offset()), reg("%rax"));
}

void X86_64Linux::bitFieldExtract(const MemberAccess &m) {
    load(m.type());
    int left = 64 - m.bitOffset() - m.width();
    int right = 64 - m.width();
    if (left > 0) a_->ins("shl", imm(left), reg("%rax"));
    a_->ins(m.type()->isSigned(target_) ? "sar" : "shr", imm(right), reg("%rax"));
}

void X86_64Linux::bitFieldInsert(const MemberAccess &m) {
    unsigned long long ones = (m.width() == 64) ? ~0ULL : ((1ULL << m.width()) - 1);
    unsigned long long mask = ones << m.bitOffset();

    a_->ins("mov", reg("%rax"), reg("%rdx"));
    a_->ins("movabs", imm(ones), reg("%rcx"));
    a_->ins("and", reg("%rcx"), reg("%rdx"));
    if (m.bitOffset() != 0) a_->ins("shl", imm(m.bitOffset()), reg("%rdx"));

    a_->ins("push", reg("%rax"));
    a_->ins("mov", reg(abi_.scratch), reg("%rax"));
    load(m.type());
    a_->ins("movabs", imm(~mask), reg("%rcx"));
    a_->ins("and", reg("%rcx"), reg("%rax"));
    a_->ins("or", reg("%rdx"), reg("%rax"));
    store(m.type());
    a_->ins("pop", reg("%rax"));

    int right = 64 - m.width();
    a_->ins("shl", imm(right), reg("%rax"));
    a_->ins(m.type()->isSigned(target_) ? "sar" : "shr", imm(right), reg("%rax"));
}

void X86_64Linux::visit(const Assign &n) {
    const MemberAccess *bf = dynamic_cast<const MemberAccess *>(&n.target());
    if (bf != nullptr && !bf->isBitField()) bf = nullptr;

    // The value is generated first and the destination address after it, which
    // is the other way round from how this reads. The reason is setjmp, and it
    // is general rather than special: that function returns twice, and the
    // second return arrives with %rsp restored to what it was when setjmp
    // recorded it - so anything this expression had pushed *before* the call is
    // read back out of stack the program has since freed and reused. Holding
    // the destination address there made 'r = setjmp(env)' store the result
    // through whatever now sat in those eight bytes, which is a wild pointer
    // rather than a wrong number. Nothing of ours may live below %rsp across
    // the call that produces the value.
    //
    // C90 leaves the order in which an assignment's two operands are evaluated
    // unspecified, so taking the address second is a choice the standard offers
    // rather than a liberty taken with it.
    n.value().accept(*this);
    bool x87 = isX87(n.type());
    bool inSse = !x87 && n.type()->isFloating();
    if (x87)          pushX87();
    else if (inSse)   pushF();
    else              push();

    if (bf) bitFieldUnitAddr(*bf);
    else    genAddr(n.target());
    a_->ins("mov", reg("%rax"), reg(abi_.scratch));

    if (x87)        popX87();
    else if (inSse) popF("%xmm0");
    else            pop("%rax");

    if (n.type()->isStructOrUnion()) {
        copyBlock(n.type()->size(target_));
        a_->ins("mov", reg(abi_.scratch), reg("%rax"));
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

    // x87 keeps the old value on its own stack rather than in a spill slot:
    // duplicate it, step the copy, store the copy, and what is left in st(0)
    // is the value the expression has.
    if (isX87(n.type())) {
        a_->ins("fld", reg("%st(0)"));
        a_->ins("fld1");
        // st(0) is the one, st(1) the value: 'fsubrp' is what subtracts the
        // one from the value, for the reason set out in genX87Binary.
        a_->ins(n.increment() ? "faddp" : "fsubrp", reg("%st"), reg("%st(1)"));
        pop(abi_.scratch);
        store(n.type());
        return;
    }

    if (n.type()->isFloating()) {
        pushF();
        bool single = genKind(n.type()) == Kind::Float;
        if (single) {
            float one = 1.0f;
            unsigned int bits;
            std::memcpy(&bits, &one, sizeof bits);
            a_->ins("mov", imm(bits), reg("%eax"));
            a_->ins("movd", reg("%eax"), reg("%xmm1"));
            a_->ins(n.increment() ? "addss" : "subss", reg("%xmm1"), reg("%xmm0"));
        } else {
            double one = 1.0;
            unsigned long long bits;
            std::memcpy(&bits, &one, sizeof bits);
            a_->ins("movabs", imm(bits), reg("%rax"));
            a_->ins("movq", reg("%rax"), reg("%xmm1"));
            a_->ins(n.increment() ? "addsd" : "subsd", reg("%xmm1"), reg("%xmm0"));
        }
        popF("%xmm1");
        pop(abi_.scratch);
        store(n.type());
        a_->ins("movapd", reg("%xmm1"), reg("%xmm0"));
        return;
    }

    push();
    a_->ins(n.increment() ? "add" : "sub", imm(n.step()), reg("%rax"));
    pop("%rdx");
    pop(abi_.scratch);
    store(n.type());
    a_->ins("mov", reg("%rdx"), reg("%rax"));
}

void X86_64Linux::visit(const Unary &n) {
    if (n.op() == '&') { genAddr(n.operand()); return; }
    if (n.op() == '*') {
        n.operand().accept(*this);
        load(n.type());
        return;
    }

    n.operand().accept(*this);
    // x87 has a sign flip and a compare against its own zero, so neither of
    // these needs the register-file shuffling the SSE forms below do.
    if (n.op() == '-' && isX87(n.type())) {
        a_->ins("fchs");
        return;
    }
    if (n.op() == '!' && isX87(n.operand().type())) {
        a_->ins("fldz");
        a_->ins("fucomip", reg("%st(1)"), reg("%st"));
        a_->ins("fstp", reg("%st(0)"));
        a_->ins("sete", reg("%al"));
        a_->ins("setnp", reg("%cl"));
        a_->ins("and", reg("%cl"), reg("%al"));
        a_->ins("movzbq", reg("%al"), reg("%rax"));
        return;
    }
    if (n.op() == '-' && n.type()->isFloating()) {
        bool isDouble = genKind(n.type()) == Kind::Double;
        a_->ins("movq", reg("%xmm0"), reg("%rax"));
        a_->ins(isDouble ? "pxor" : "pxor", reg("%xmm0"), reg("%xmm0"));
        a_->ins("movq", reg("%rax"), reg("%xmm1"));
        a_->ins(isDouble ? "subsd" : "subss", reg("%xmm1"), reg("%xmm0"));
    } else if (n.op() == '-') {
        a_->ins("neg", reg(acc(n.type())));
        canonicalise(n.type());
    } else if (n.op() == '!' && n.operand().type()->isFloating()) {
        bool isDouble = genKind(n.operand().type()) == Kind::Double;
        a_->ins("pxor", reg("%xmm1"), reg("%xmm1"));
        a_->ins(isDouble ? "ucomisd" : "ucomiss", reg("%xmm1"), reg("%xmm0"));
        // !NaN is 0, because NaN is true. Unordered sets ZF and sete alone
        // would call it zero, so PF has to be consulted here too.
        a_->ins("sete", reg("%al"));
        a_->ins("setnp", reg("%cl"));
        a_->ins("and", reg("%cl"), reg("%al"));
        a_->ins("movzbq", reg("%al"), reg("%rax"));
    } else if (n.op() == '!') {
        a_->ins("cmp", immText("0"), reg("%rax"));
        a_->ins("sete", reg("%al"));
        a_->ins("movzbq", reg("%al"), reg("%rax"));
    }
}

// x87 rounds to nearest by default and C requires a cast to truncate, so the
// control word is saved, the two rounding bits are set to 'toward zero', the
// store happens, and the old word goes back. There is no truncating store to
// undo this for us the way cvttsd2si does on the SSE side.
void X86_64Linux::x87ToInt(const Type *to) {
    a_->ins("sub", immText("16"), reg("%rsp"));
    a_->ins("fnstcw", mem(12, "%rsp"));
    a_->ins("movzwl", mem(12, "%rsp"), reg("%eax"));
    a_->ins("or", immText("0x0c00"), reg("%eax"));
    a_->ins("mov", reg("%ax"), mem(10, "%rsp"));
    a_->ins("fldcw", mem(10, "%rsp"));
    a_->ins("fistpq", mem("%rsp"));
    a_->ins("fldcw", mem(12, "%rsp"));
    a_->ins("mov", mem("%rsp"), reg("%rax"));
    a_->ins("add", immText("16"), reg("%rsp"));
    canonicalise(to);
}

// The reverse. fild reads a *signed* integer, so an unsigned 64-bit value with
// its top bit set would arrive negative; it is loaded as if signed and 2^64 is
// added back when it was. Narrower unsigned types cannot reach that bit, since
// every integer here is already widened to 64 bits for its own type.
void X86_64Linux::intToX87(const Type *from) {
    bool wideUnsigned = from->size(target_) == 8 && !from->isSigned(target_);
    a_->ins("sub", immText("16"), reg("%rsp"));
    a_->ins("mov", reg("%rax"), mem("%rsp"));
    a_->ins("fildq", mem("%rsp"));
    if (wideUnsigned) {
        int id = nextLabel();
        // 2^64 as an 80-bit constant: exponent 64 biased by 16383, leading one.
        a_->ins("cmp", immText("0"), reg("%rax"));
        a_->ins("jns", lbl(label("nofix", id)));
        a_->ins("movabs", imm(0x8000000000000000ULL), reg("%rax"));
        a_->ins("mov", reg("%rax"), mem("%rsp"));
        a_->ins("movw", imm((16383 + 64)), mem(8, "%rsp"));
        a_->ins("fldt", mem("%rsp"));
        a_->ins("faddp", reg("%st"), reg("%st(1)"));
        a_->defLabel(label("nofix", id));
    }
    a_->ins("add", immText("16"), reg("%rsp"));
}

void X86_64Linux::genConversion(const Type *from, const Type *to) {
    if (to->isVoid()) {
        // A long double discarded still has to leave the x87 stack, or eight
        // such casts in a row overflow it and every later load reads NaN.
        if (isX87(from)) a_->ins("fstp", reg("%st(0)"));
        return;
    }

    bool fromF = from->isFloating(), toF = to->isFloating();

    if (!fromF && !toF) { canonicalise(to); return; }

    // Into and out of x87, which is a move between register files and not a
    // conversion instruction: fld reads the narrower format and widens, fstp
    // writes it back and rounds.
    if (isX87(to) && !isX87(from)) {
        if (!fromF) { intToX87(from); return; }
        a_->ins("sub", immText("16"), reg("%rsp"));
        if (genKind(from) == Kind::Float) {
            a_->ins("movss", reg("%xmm0"), mem("%rsp"));
            a_->ins("flds", mem("%rsp"));
        } else {
            a_->ins("movsd", reg("%xmm0"), mem("%rsp"));
            a_->ins("fldl", mem("%rsp"));
        }
        a_->ins("add", immText("16"), reg("%rsp"));
        return;
    }
    if (isX87(from) && !isX87(to)) {
        if (!toF) { x87ToInt(to); return; }
        a_->ins("sub", immText("16"), reg("%rsp"));
        if (genKind(to) == Kind::Float) {
            a_->ins("fstps", mem("%rsp"));
            a_->ins("movss", mem("%rsp"), reg("%xmm0"));
        } else {
            a_->ins("fstpl", mem("%rsp"));
            a_->ins("movsd", mem("%rsp"), reg("%xmm0"));
        }
        a_->ins("add", immText("16"), reg("%rsp"));
        return;
    }
    if (isX87(from) && isX87(to)) return;

    if (fromF && toF) {
        if (genKind(from) == genKind(to)) return;
        if (genKind(to) == Kind::Double) a_->ins("cvtss2sd", reg("%xmm0"), reg("%xmm0"));
        else                             a_->ins("cvtsd2ss", reg("%xmm0"), reg("%xmm0"));
        return;
    }

    if (!fromF && toF) {
        const char *op = genKind(to) == Kind::Double ? "cvtsi2sdq" : "cvtsi2ssq";
        a_->ins(op, reg("%rax"), reg("%xmm0"));
        return;
    }

    const char *op = genKind(from) == Kind::Double ? "cvttsd2si" : "cvttss2si";
    a_->ins(op, reg("%xmm0"), reg("%rax"));
    canonicalise(to);
}

void X86_64Linux::visit(const Cast &n) {
    n.value().accept(*this);
    genConversion(n.value().type(), n.type());
}

// The x87 twin of genFloatBinary. Both operands go through memory and are
// loaded back so that st(0) is the left and st(1) the right, matching the SSE
// path where %xmm0 is the left - which is what lets the comparison table below
// be the same one, NaN reasoning included: fucomip sets ZF, PF and CF exactly
// as ucomis does.
//
// The two operators that are not commutative are spelled the way GNU as reads
// them, which is the *opposite* of the Intel manual's sense: written here,
// 'fsubp %st, %st(1)' computes st(0) - st(1), and 'fsubrp' computes
// st(1) - st(0). That is the old AT&T reversal of fsub and fsubr, and it is
// not guesswork - the first version of this function used the other pair and
// the suite answered '7.0L - 2.0L' with -5 and '7.0L / 2.0L' with 0.2857.
// Add and multiply cannot ask the question.
void X86_64Linux::genX87Binary(const Binary &n) {
    n.lhs().accept(*this);
    pushX87();
    n.rhs().accept(*this);
    // st(0) is the right; bring the left back above it.
    a_->ins("fldt", mem("%rsp"));
    a_->ins("add", immText("16"), reg("%rsp"));
    depth_ -= 2;

    switch (n.op()) {
    case BinOp::Add: a_->ins("faddp", reg("%st"), reg("%st(1)")); return;
    case BinOp::Sub: a_->ins("fsubp", reg("%st"), reg("%st(1)")); return;
    case BinOp::Mul: a_->ins("fmulp", reg("%st"), reg("%st(1)")); return;
    case BinOp::Div: a_->ins("fdivp", reg("%st"), reg("%st(1)")); return;
    default: break;
    }

    const char *set = nullptr;
    bool swapped = false;
    switch (n.op()) {
    case BinOp::Eq: case BinOp::Ne: break;
    case BinOp::Lt: set = "seta";  swapped = true; break;
    case BinOp::Le: set = "setae"; swapped = true; break;
    case BinOp::Gt: set = "seta";  break;
    case BinOp::Ge: set = "setae"; break;
    default:
        std::fprintf(stderr, "codegen: that operator has no floating form\n");
        std::exit(1);
    }

    // fucomip compares st(0) with st(1) and pops one; the other is dropped
    // after, because a comparison must leave the stack as empty as it found it.
    if (swapped) a_->ins("fxch", reg("%st(1)"));
    a_->ins("fucomip", reg("%st(1)"), reg("%st"));
    a_->ins("fstp", reg("%st(0)"));

    if (n.op() == BinOp::Eq) {
        a_->ins("sete", reg("%al"));
        a_->ins("setnp", reg("%cl"));
        a_->ins("and", reg("%cl"), reg("%al"));
    } else if (n.op() == BinOp::Ne) {
        a_->ins("setne", reg("%al"));
        a_->ins("setp", reg("%cl"));
        a_->ins("or", reg("%cl"), reg("%al"));
    } else {
        a_->ins(set, reg("%al"));
    }
    a_->ins("movzbq", reg("%al"), reg("%rax"));
}

void X86_64Linux::genFloatBinary(const Binary &n) {
    const Type *t = n.lhs().type();
    if (isX87(t)) { genX87Binary(n); return; }
    bool isDouble = genKind(t) == Kind::Double;
    const char *sfx = isDouble ? "sd" : "ss";

    n.rhs().accept(*this);
    pushF();
    n.lhs().accept(*this);
    popF("%xmm1");

    switch (n.op()) {
    case BinOp::Add: a_->ins(std::string("add") + sfx, reg("%xmm1"), reg("%xmm0")); return;
    case BinOp::Sub: a_->ins(std::string("sub") + sfx, reg("%xmm1"), reg("%xmm0")); return;
    case BinOp::Mul: a_->ins(std::string("mul") + sfx, reg("%xmm1"), reg("%xmm0")); return;
    case BinOp::Div: a_->ins(std::string("div") + sfx, reg("%xmm1"), reg("%xmm0")); return;
    default: break;
    }

    // NaN is why none of this is the obvious spelling. ucomis sets ZF, PF and
    // CF *all* to one when either operand is NaN, so sete reads "equal" and
    // setb reads "less" for a value that is neither - and IEEE says every
    // comparison against NaN is false except !=.
    //
    // seta and setae need CF clear, which unordered never gives, so > and >=
    // are already right. < and <= are obtained by comparing the other way
    // round and using them, rather than by setb, which unordered would satisfy.
    // Only == and != have to consult PF directly.
    const char *set = nullptr;
    bool swapped = false;
    switch (n.op()) {
    case BinOp::Eq: case BinOp::Ne: break;
    case BinOp::Lt: set = "seta";  swapped = true; break;
    case BinOp::Le: set = "setae"; swapped = true; break;
    case BinOp::Gt: set = "seta";  break;
    case BinOp::Ge: set = "setae"; break;
    default:
        std::fprintf(stderr, "codegen: that operator has no floating form\n");
        std::exit(1);
    }

    if (swapped) a_->ins(std::string("ucomi") + sfx, reg("%xmm0"), reg("%xmm1"));
    else         a_->ins(std::string("ucomi") + sfx, reg("%xmm1"), reg("%xmm0"));

    if (n.op() == BinOp::Eq) {
        // equal and ordered
        a_->ins("sete", reg("%al"));
        a_->ins("setnp", reg("%cl"));
        a_->ins("and", reg("%cl"), reg("%al"));
    } else if (n.op() == BinOp::Ne) {
        // not equal, or unordered
        a_->ins("setne", reg("%al"));
        a_->ins("setp", reg("%cl"));
        a_->ins("or", reg("%cl"), reg("%al"));
    } else {
        a_->ins(set, reg("%al"));
    }
    a_->ins("movzbq", reg("%al"), reg("%rax"));
}

void X86_64Linux::visit(const Binary &n) {
    if (n.op() == BinOp::LAnd || n.op() == BinOp::LOr) {
        int id = nextLabel();
        bool isAnd = n.op() == BinOp::LAnd;
        const char *shortJump = isAnd ? "je" : "jne";

        genTruth(n.lhs());
        a_->ins("cmp", immText("0"), reg("%rax"));
        a_->ins(shortJump, lbl(label("sc", id)));
        genTruth(n.rhs());
        a_->ins("cmp", immText("0"), reg("%rax"));
        a_->ins(shortJump, lbl(label("sc", id)));
        a_->ins("mov", imm((isAnd ? 1 : 0)), reg("%rax"));
        a_->ins("jmp", lbl(label("scend", id)));
        a_->defLabel(label("sc", id));
        a_->ins("mov", imm((isAnd ? 0 : 1)), reg("%rax"));
        a_->defLabel(label("scend", id));
        return;
    }

    if (n.lhs().type()->isFloating()) { genFloatBinary(n); return; }

    n.rhs().accept(*this);
    push();
    n.lhs().accept(*this);
    pop(abi_.scratch);

    const Type *t = n.lhs().type();
    const char *a = acc(t);
    const char *d = rhs(t);
    bool sign = t->isSigned(target_);
    bool wide = t->size(target_) == 8;

    switch (n.op()) {
    case BinOp::Add: a_->ins("add", reg(d), reg(a)); canonicalise(n.type()); return;
    case BinOp::Sub: a_->ins("sub", reg(d), reg(a)); canonicalise(n.type()); return;
    case BinOp::Mul: a_->ins("imul", reg(d), reg(a)); canonicalise(n.type()); return;
    case BinOp::BitAnd: a_->ins("and", reg(d), reg(a)); canonicalise(n.type()); return;
    case BinOp::BitOr:  a_->ins("or", reg(d), reg(a)); canonicalise(n.type()); return;
    case BinOp::BitXor: a_->ins("xor", reg(d), reg(a)); canonicalise(n.type()); return;

    case BinOp::Div:
    case BinOp::Mod:
        if (sign) { if (wide) a_->ins("cqo"); else a_->ins("cdq"); }
        else      a_->ins("xor", reg("%edx"), reg("%edx"));
        a_->ins(sign ? "idiv" : "div", reg(d));
        if (n.op() == BinOp::Mod) { if (wide) a_->ins("mov", reg("%rdx"), reg("%rax")); else a_->ins("mov", reg("%edx"), reg("%eax")); }
        canonicalise(n.type());
        return;

    case BinOp::Shl:
    case BinOp::Shr:
        a_->ins("mov", reg(abi_.scratch), reg("%rcx"));
        if (n.op() == BinOp::Shl) a_->ins("shl", reg("%cl"), reg(a));
        else                      a_->ins(sign ? "sar" : "shr", reg("%cl"), reg(a));
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
    a_->ins("cmp", reg(d), reg(a));
    a_->ins(set, reg("%al"));
    a_->ins("movzbq", reg("%al"), reg("%rax"));
}

void X86_64Linux::visit(const Call &n) {
    std::vector<std::vector<bool> > isSse;
    std::vector<std::vector<int> > slot;
    std::vector<bool> onStack;
    // Eight bytes of padding placed *below* this argument, so that what
    // follows starts sixteen-aligned. Only x87 needs it: %rsp is sixteen-
    // aligned at the call, so a stack argument is aligned exactly when its
    // slot index is even, and a long double after a single int would not be.
    // glibc's own va_arg rounds the overflow pointer up to sixteen before
    // reading one, so printf("%Lf") reads the wrong sixteen bytes without this.
    std::vector<bool> padBelow;
    // A MEMORY-class return spends %rdi on the hidden pointer before any
    // argument is placed, so every integer argument shifts along by one.
    bool byRef = abi_.aggregatesByReference;
    bool sret = n.type()->isStructOrUnion() &&
               (containsX87(n.type(), target_) ||
                (byRef ? !msInRegister(n.type()->size(target_))
                       : n.type()->size(target_) > abi_.structReturnLimit));
    int ints = sret ? 1 : 0, sses = sret && abi_.positional ? 1 : 0;
    int stackSlots = 0;
    for (const ExprPtr &arg : n.args()) {
        const Type *t = arg->type();
        std::vector<bool> lanes;
        bool memory;
        if (byRef && t->isStructOrUnion()) {
            // One integer slot either way: the bytes when they fit a register,
            // otherwise the address of the caller's copy. Never a vector
            // register - this ABI does not put aggregates there at all.
            lanes.push_back(false);
            memory = ints + 1 > abi_.intCount;
            if (memory) lanes.clear();
            std::vector<int> regs;
            if (memory) stackSlots += 1;
            else        regs.push_back(takeSlot(false, ints, sses));
            onStack.push_back(memory);
            isSse.push_back(lanes);
            slot.push_back(regs);
            padBelow.push_back(false);
            continue;
        }
        // System V gives 'long double' the classes X87 and X87UP, and neither
        // names a register the caller may put it in: it goes on the stack
        // always, and an aggregate carrying one goes there whole. Decided
        // ahead of the counting below because it spends no register file, so
        // a call may still fill all six integer registers around it.
        memory = isX87(t) || containsX87(t, target_) ||
                 (t->isStructOrUnion() &&
                  t->size(target_) > abi_.structReturnLimit);
        if (!memory) {
            if (t->isStructOrUnion()) lanes = classifyEightbytes(t, target_);
            else                      lanes.push_back(t->isFloating());
            int wantInt = 0, wantSse = 0;
            for (bool sse : lanes) { if (sse) wantSse++; else wantInt++; }
            memory = ints + wantInt > abi_.intCount ||
                     sses + wantSse > abi_.sseCount;
        }

        std::vector<int> regs;
        bool pad = false;
        if (memory) {
            lanes.clear();
            int size = (t->isStructOrUnion() || isX87(t)) ? t->size(target_) : 8;
            if (t->align(target_) >= 16 && stackSlots % 2 != 0) {
                pad = true;
                stackSlots += 1;
            }
            stackSlots += (size + 7) / 8;
        } else {
            for (bool sse : lanes) regs.push_back(takeSlot(sse, ints, sses));
        }
        onStack.push_back(memory);
        isSse.push_back(lanes);
        slot.push_back(regs);
        padBelow.push_back(pad);
    }

    int shadowSlots = abi_.shadowBytes / 8;

    // Counted before anything is pushed: the memory arguments sit on the stack
    // the call has to find aligned, so deciding this afterwards is wrong.
    int padSlots = ((depth_ + stackSlots + shadowSlots) % 2 != 0) ? 1 : 0;
    if (padSlots) { a_->ins("sub", immText("8"), reg("%rsp")); depth_++; }

    // Reverse: push moves down, and the first memory argument must end lowest.
    // The padding an argument asked for goes on *after* its own bytes, since
    // pushing moves toward lower addresses and the pad belongs beneath it.
    for (std::size_t i = n.args().size(); i-- > 0; ) {
        if (!onStack[i]) continue;
        const Type *t = n.args()[i]->type();
        n.args()[i]->accept(*this);
        if (byRef && t->isStructOrUnion()) {
            msAggregateToRax(t, n.argSlot(i));
            push();
            if (padBelow[i]) { a_->ins("sub", immText("8"), reg("%rsp")); depth_++; }
            continue;
        }
        if (!t->isStructOrUnion()) {
            if (isX87(t))             pushX87();
            else if (t->isFloating()) pushF();
            else                      push();
            if (padBelow[i]) { a_->ins("sub", immText("8"), reg("%rsp")); depth_++; }
            continue;
        }
        int size = t->size(target_);
        int slots = (size + 7) / 8;
        a_->ins("mov", reg("%rax"), reg("%rcx"));
        for (int k = slots; k-- > 0; ) {
            int off = k * 8;
            int left = size - off;
            if (left >= 8)      a_->ins("mov", mem(off, "%rcx"), reg("%rax"));
            else if (left >= 4) a_->ins("movl", mem(off, "%rcx"), reg("%eax"));
            else if (left >= 2) a_->ins("movzwl", mem(off, "%rcx"), reg("%eax"));
            else                a_->ins("movzbl", mem(off, "%rcx"), reg("%eax"));
            push();
        }
        if (padBelow[i]) { a_->ins("sub", immText("8"), reg("%rsp")); depth_++; }
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
            if (isSse[i][0]) {
                // Microsoft x64 sends a variadic float in both files - the
                // vector register and the integer register of the same slot -
                // because the callee has no prototype to tell it which one to
                // read. printf reads the integer twin.
                if (abi_.positional && n.isVariadic() &&
                    static_cast<int>(i) >= n.namedArgs())
                    a_->ins("mov", mem("%rsp"), reg(abi_.intRegs[slot[i][0]]));
                popF(abi_.sseRegs[slot[i][0]]);
            } else {
                pop(abi_.intRegs[slot[i][0]]);
            }
            continue;
        }
        pop("%rax");
        if (byRef) {
            msAggregateToRax(t, n.argSlot(i));
            a_->ins("mov", reg("%rax"), reg(abi_.intRegs[slot[i][0]]));
            continue;
        }
        int size = t->size(target_);
        for (std::size_t k = 0; k < isSse[i].size(); k++) {
            int off = static_cast<int>(k) * 8;
            int left = size - off;
            if (isSse[i][k]) {
                a_->ins(left >= 8 ? "movsd" : "movss", mem(off, "%rax"), reg(abi_.sseRegs[slot[i][k]]));
            } else if (left >= 8) {
                a_->ins("mov", mem(off, "%rax"), reg(abi_.intRegs[slot[i][k]]));
            } else {
                // %r11 and not %rcx: this loop runs last argument to first, so
                // the registers after this one are already live, and %rcx is
                // the fourth of them under System V and the first under Windows.
                if (left >= 4)      a_->ins("movl", mem(off, "%rax"), reg("%r11d"));
                else if (left >= 2) a_->ins("movzwl", mem(off, "%rax"), reg("%r11d"));
                else                a_->ins("movzbl", mem(off, "%rax"), reg("%r11d"));
                a_->ins("mov", reg("%r11"), reg(abi_.intRegs[slot[i][k]]));
            }
        }
    }
    // %r11, not %rax: %rax is written just below with the variadic SSE count.
    if (n.callee() != nullptr) pop("%r11");

    if (sret) a_->ins("lea", mem((-n.resultSlot()), "%rbp"), reg(abi_.intRegs[0]));

    a_->ins("mov", imm(((n.isVariadic() && abi_.variadicSseCountInAl) ? sses : 0)), reg("%rax"));

    // Opened last, after every temporary push has been popped back off, so the
    // memory arguments end up above it: the callee reads them from 32(%rsp)
    // upwards and spills its register arguments into what is below.
    if (shadowSlots > 0) {
        a_->ins("sub", imm(abi_.shadowBytes), reg("%rsp"));
        depth_ += shadowSlots;
    }

    if (n.callee() != nullptr) a_->ins("call", ind("%r11"));
    else                       a_->ins("call", lbl(n.name()));

    int unwind = stackSlots + padSlots + shadowSlots;
    if (unwind > 0) {
        a_->ins("add", imm(unwind * 8), reg("%rsp"));
        depth_ -= unwind;
    }

    if (sret) {
        a_->ins("lea", mem((-n.resultSlot()), "%rbp"), reg("%rax"));
        return;
    }

    if (byRef && n.type()->isStructOrUnion()) {
        // Not sret, so it came back in %rax as bytes - store them where the
        // result lives and hand back its address, as every other path does.
        int size = n.type()->size(target_);
        int to = -n.resultSlot();
        if (size == 8)      a_->ins("mov", reg("%rax"), mem(to, "%rbp"));
        else if (size == 4) a_->ins("movl", reg("%eax"), mem(to, "%rbp"));
        else if (size == 2) a_->ins("movw", reg("%ax"), mem(to, "%rbp"));
        else                a_->ins("movb", reg("%al"), mem(to, "%rbp"));
        a_->ins("lea", mem(to, "%rbp"), reg("%rax"));
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
                a_->ins(left >= 8 ? "movsd" : "movss", reg(sret[nextSse++]), mem(off, "%rbp"));
            } else {
                const char *r = ret[nextInt++];
                if (left >= 8)      a_->ins("mov", reg(r), mem(off, "%rbp"));
                else if (left >= 4) a_->ins("movl", reg(narrower(r, 4)), mem(off, "%rbp"));
                else if (left >= 2) a_->ins("movw", reg(narrower(r, 2)), mem(off, "%rbp"));
                else                a_->ins("movb", reg(narrower(r, 1)), mem(off, "%rbp"));
            }
        }
        a_->ins("lea", mem((-base), "%rbp"), reg("%rax"));
        return;
    }

    if (!n.type()->isVoid() && !n.type()->isFloating()) canonicalise(n.type());
}

void X86_64Linux::genTruth(const Expr &e) {
    e.accept(*this);
    if (isX87(e.type())) {
        a_->ins("fldz");
        a_->ins("fucomip", reg("%st(1)"), reg("%st"));
        a_->ins("fstp", reg("%st(0)"));
        a_->ins("setne", reg("%al"));
        a_->ins("setp", reg("%cl"));
        a_->ins("or", reg("%cl"), reg("%al"));
        a_->ins("movzbq", reg("%al"), reg("%rax"));
        return;
    }
    if (!e.type()->isFloating()) return;
    bool isDouble = genKind(e.type()) == Kind::Double;
    a_->ins("pxor", reg("%xmm1"), reg("%xmm1"));
    a_->ins(isDouble ? "ucomisd" : "ucomiss", reg("%xmm1"), reg("%xmm0"));
    // NaN is not equal to zero, so it is true - and unordered sets ZF, which
    // setne alone would read as false. PF is what tells the two apart.
    a_->ins("setne", reg("%al"));
    a_->ins("setp", reg("%cl"));
    a_->ins("or", reg("%cl"), reg("%al"));
    a_->ins("movzbq", reg("%al"), reg("%rax"));
}

// Fetch one argument and step the walk. Both conventions end the same way -
// an address in %rax, handed to load() - and differ entirely in how far the
// walk has to look to find it.
//
// Microsoft x64 puts every argument, named or not, in a consecutive eight-byte
// slot, and a variadic double arrives in the integer register as well as the
// vector one. So the va_list is a pointer, one slot is always eight bytes, and
// the type only decides how to read what is there.
//
// System V spilled the register arguments into a save area and left the rest
// on the stack, so the walk has to ask which of the two an argument is in.
// gp_offset counts up to 48 - six integer registers - and fp_offset from 48 to
// 176, eight vector ones at sixteen bytes of slot each. Past either limit the
// argument was never in a register and comes from overflow_arg_area, which
// steps by eight for both, because the stack does not keep the vector
// registers' wider slots.
void X86_64Linux::visit(const VaArg &n) {
    n.list().accept(*this);                     // %rax = the va_list
    const Type *t = n.type();

    if (abi_.positional) {
        a_->ins("mov", mem("%rax"), reg("%rcx"));         // the next slot
        a_->ins("lea", mem(8, "%rcx"), reg("%rdx"));
        a_->ins("mov", reg("%rdx"), mem("%rax"));
        a_->ins("mov", reg("%rcx"), reg("%rax"));
        load(t);
        return;
    }

    // A long double is class X87/X87UP and is never put in the register save
    // area, so there is no register case to test: it is always in the overflow
    // area, sixteen-aligned there and sixteen wide.
    if (isX87(t)) {
        a_->ins("mov", mem(8, "%rax"), reg("%rdx"));          // overflow_arg_area
        a_->ins("add", immText("15"), reg("%rdx"));
        a_->ins("and", immText("-16"), reg("%rdx"));
        a_->ins("lea", mem(16, "%rdx"), reg("%rcx"));
        a_->ins("mov", reg("%rcx"), mem(8, "%rax"));
        a_->ins("mov", reg("%rdx"), reg("%rax"));
        load(t);
        return;
    }

    bool sse = t->isFloating();
    int id = nextLabel();
    std::string onStack = label("va.stack", id);
    std::string done = label("va.done", id);

    // The offset for this argument's register file, and the limit past which
    // the file is spent.
    a_->ins("movl", (sse ? mem(4, "%rax") : mem("%rax")), reg("%ecx"));
    a_->ins("cmpl", imm((sse ? 176 : 48)), reg("%ecx"));
    a_->ins("jae", lbl(onStack));

    a_->ins("mov", mem(16, "%rax"), reg("%rdx"));           // reg_save_area
    a_->ins("add", reg("%rcx"), reg("%rdx"));
    a_->ins("addl", imm((sse ? 16 : 8)), reg("%ecx"));
    a_->ins("movl", reg("%ecx"), (sse ? mem(4, "%rax") : mem("%rax")));
    a_->ins("jmp", lbl(done));

    a_->defLabel(onStack);
    a_->ins("mov", mem(8, "%rax"), reg("%rdx"));            // overflow_arg_area
    a_->ins("lea", mem(8, "%rdx"), reg("%rcx"));
    a_->ins("mov", reg("%rcx"), mem(8, "%rax"));

    a_->defLabel(done);
    a_->ins("mov", reg("%rdx"), reg("%rax"));
    load(t);
}

void X86_64Linux::visit(const VaStart &n) {
    n.list().accept(*this);
    if (abi_.positional) {
        a_->ins("lea", mem(varOverflow_, "%rbp"), reg("%rcx"));
        a_->ins("mov", reg("%rcx"), mem("%rax"));
        return;
    }
    a_->ins("movl", imm(varGp_), mem("%rax"));
    a_->ins("movl", imm(varFp_), mem(4, "%rax"));
    a_->ins("lea", mem(varOverflow_, "%rbp"), reg("%rcx"));
    a_->ins("mov", reg("%rcx"), mem(8, "%rax"));
    a_->ins("lea", mem((-regSave_), "%rbp"), reg("%rcx"));
    a_->ins("mov", reg("%rcx"), mem(16, "%rax"));
}

void X86_64Linux::visit(const ExprStmt &n) { n.expr().accept(*this); }

void X86_64Linux::visit(const Return &n) {
    if (!n.hasValue()) {
        a_->ins("jmp", lbl(returnLabel_));
        return;
    }
    n.value().accept(*this);

    if (sretSlot_ != 0) {
        a_->ins("mov", mem(-(sretSlot_), "%rbp"), reg(abi_.scratch));
        copyBlock(n.value().type()->size(target_));
        a_->ins("mov", mem(-(sretSlot_), "%rbp"), reg("%rax"));
        a_->ins("jmp", lbl(returnLabel_));
        return;
    }

    if (n.value().type()->isStructOrUnion()) {
        const Type *t = n.value().type();
        int size = t->size(target_);

        // Microsoft x64 hands back an aggregate small enough for a register in
        // %rax, as bytes, whatever those bytes mean. There is no vector-register
        // case: 'struct { double x; }' comes back in %rax where a bare double
        // comes back in %xmm0, and the difference is the struct rather than the
        // member. System V is the one that classifies, and doing that here on
        // both targets returned a one-double struct in %xmm0 while the caller -
        // which had the Microsoft rule right - read %rax.
        if (abi_.aggregatesByReference) {
            a_->ins("mov", reg("%rax"), reg("%rcx"));
            if (size == 8)      a_->ins("mov", mem("%rcx"), reg("%rax"));
            else if (size == 4) a_->ins("movl", mem("%rcx"), reg("%eax"));
            else if (size == 2) a_->ins("movzwl", mem("%rcx"), reg("%eax"));
            else                a_->ins("movzbl", mem("%rcx"), reg("%eax"));
            a_->ins("jmp", lbl(returnLabel_));
            return;
        }

        std::vector<bool> lanes = classifyEightbytes(t, target_);
        const char *ret[2] = { "%rax", "%rdx" };
        const char *sret[2] = { "%xmm0", "%xmm1" };
        int nextInt = 0, nextSse = 0;
        a_->ins("mov", reg("%rax"), reg("%rcx"));
        for (std::size_t k = 0; k < lanes.size(); k++) {
            int off = static_cast<int>(k) * 8;
            int left = size - off;
            if (lanes[k]) {
                a_->ins(left >= 8 ? "movsd" : "movss", mem(off, "%rcx"), reg(sret[nextSse++]));
            } else if (left >= 8) {
                a_->ins("mov", mem(off, "%rcx"), reg(ret[nextInt++]));
            } else {
                const char *r = ret[nextInt++];
                const char *e = narrower(r, 4);
                if (left >= 4)      a_->ins("movl", mem(off, "%rcx"), reg(e));
                else if (left >= 2) a_->ins("movzwl", mem(off, "%rcx"), reg(e));
                else                a_->ins("movzbl", mem(off, "%rcx"), reg(e));
            }
        }
    }
    a_->ins("jmp", lbl(returnLabel_));
}

void X86_64Linux::visit(const Block &n) {
    for (const StmtPtr &s : n.body()) s->accept(*this);
}

void X86_64Linux::visit(const If &n) {
    int id = nextLabel();
    genTruth(n.cond());
    a_->ins("cmp", immText("0"), reg("%rax"));
    if (n.elseArm()) {
        a_->ins("je", lbl(label("else", id)));
        n.thenArm().accept(*this);
        a_->ins("jmp", lbl(label("end", id)));
        a_->defLabel(label("else", id));
        n.elseArm()->accept(*this);
    } else {
        a_->ins("je", lbl(label("end", id)));
        n.thenArm().accept(*this);
    }
    a_->defLabel(label("end", id));
}

void X86_64Linux::visit(const While &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("begin", id) });
    a_->defLabel(label("begin", id));
    genTruth(n.cond());
    a_->ins("cmp", immText("0"), reg("%rax"));
    a_->ins("je", lbl(label("end", id)));
    n.body().accept(*this);
    a_->ins("jmp", lbl(label("begin", id)));
    a_->defLabel(label("end", id));
    jumps_.pop_back();
}

void X86_64Linux::visit(const For &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    if (n.init()) n.init()->accept(*this);
    a_->defLabel(label("begin", id));
    if (n.cond()) {
        genTruth(*n.cond());
        a_->ins("cmp", immText("0"), reg("%rax"));
        a_->ins("je", lbl(label("end", id)));
    }
    n.body().accept(*this);
    a_->defLabel(label("step", id));
    if (n.step()) n.step()->accept(*this);
    a_->ins("jmp", lbl(label("begin", id)));
    a_->defLabel(label("end", id));

    jumps_.pop_back();
}

void X86_64Linux::visit(const DoWhile &n) {
    int id = nextLabel();
    jumps_.push_back({ label("end", id), label("step", id) });

    a_->defLabel(label("begin", id));
    n.body().accept(*this);
    a_->defLabel(label("step", id));
    genTruth(n.cond());
    a_->ins("cmp", immText("0"), reg("%rax"));
    a_->ins("jne", lbl(label("begin", id)));
    a_->defLabel(label("end", id));

    jumps_.pop_back();
}

void X86_64Linux::visit(const Switch &n) {
    int id = nextLabel();

    n.cond().accept(*this);
    for (const Case *c : n.cases()) {
        long long v = c->value();
        if (v >= -2147483648L && v <= 2147483647L) {
            a_->ins("cmp", imm(v), reg("%rax"));
        } else {
            a_->ins("movabs", imm(v), reg("%rdx"));
            a_->ins("cmp", reg("%rdx"), reg("%rax"));
        }
        a_->ins("je", lbl(label("case", c->id())));
    }
    a_->ins("jmp", lbl((n.defaultCase() ? label("default", n.defaultCase()->id())
                             : label("end", id))));

    jumps_.push_back({ label("end", id), "" });
    n.body().accept(*this);
    jumps_.pop_back();
    a_->defLabel(label("end", id));
}

void X86_64Linux::visit(const Case &n) {
    a_->defLabel(label(n.isDefault() ? "default" : "case", n.id()));
    n.body().accept(*this);
}

void X86_64Linux::visit(const Goto &n) {
    a_->ins("jmp", lbl(userLabel(n.label())));
}

void X86_64Linux::visit(const Label &n) {
    a_->defLabel(userLabel(n.name()));
    n.body().accept(*this);
}

void X86_64Linux::visit(const Conditional &n) {
    int id = nextLabel();
    genTruth(n.cond());
    a_->ins("cmp", immText("0"), reg("%rax"));
    a_->ins("je", lbl(label("else", id)));
    n.thenArm().accept(*this);
    a_->ins("jmp", lbl(label("end", id)));
    a_->defLabel(label("else", id));
    n.elseArm().accept(*this);
    a_->defLabel(label("end", id));
}

void X86_64Linux::visit(const Comma &n) {
    n.left().accept(*this);
    n.right().accept(*this);
}

void X86_64Linux::visit(const Break &) {
    a_->ins("jmp", lbl(jumps_.back().brk));
}

void X86_64Linux::visit(const Continue &) {
    for (std::size_t i = jumps_.size(); i-- > 0;) {
        if (!jumps_[i].cont.empty()) {
            a_->ins("jmp", lbl(jumps_[i].cont));
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
    chunks_.push_back(out_);
    out_.clear();
}

void X86_64Linux::emit(const Function &fn) {
    depth_ = 0;
    labels_ = 0;
    labelPrefix_ = ".L." + fn.name() + ".";
    returnLabel_ = ".L.return." + fn.name();

    a_->functionBegin(fn.name(), !fn.isStatic());
    a_->prologue(fn.frameSize());

    sretSlot_ = fn.sretSlot();
    if (sretSlot_ != 0)
        a_->ins("mov", reg(abi_.intRegs[0]), mem(-(sretSlot_), "%rbp"));

    // Before the named parameters are read out, because reading them destroys
    // the registers this has to preserve. %al carries how many vector
    // registers the caller actually used, and a caller that used none may have
    // left rubbish in them - so the vector half is skipped rather than
    // faulting on a caller that passed no floating point at all.
    // Microsoft x64 needs no save area at all. Every argument, named or not,
    // owns a consecutive eight-byte slot from 16(%rbp) up, and the first four
    // of those slots are the shadow space the caller already left - so the
    // callee spills its registers into a place that exists, and the walk is
    // one pointer.
    regSave_ = fn.regSaveSlot();
    if (fn.isVariadic() && abi_.positional) {
        for (int i = 0; i < abi_.intCount; i++)
            a_->ins("mov", reg(abi_.intRegs[i]), mem((16 + i * 8), "%rbp"));
        regSave_ = 0;
    } else if (regSave_ != 0) {
        for (int i = 0; i < 6; i++)
            a_->ins("mov", reg(abi_.intRegs[i]), mem((i * 8 - regSave_), "%rbp"));
        std::string done = ".L.novec." + fn.name();
        a_->ins("testb", reg("%al"), reg("%al"));
        a_->ins("je", lbl(done));
        for (int i = 0; i < 8; i++)
            a_->ins("movaps", reg("%xmm" + std::to_string(i)), mem((48 + i * 16 - regSave_), "%rbp"));
        a_->defLabel(done);
    }

    const std::vector<Param> &ps = fn.params();
    // Starts at 1 for a MEMORY return, exactly as the caller's count does.
    int ints = (sretSlot_ != 0) ? 1 : 0;
    int sses = (sretSlot_ != 0 && abi_.positional) ? 1 : 0;
    // 16 clears the saved %rbp and the return address. Windows adds the shadow
    // area on top of those, so its first memory argument is that much further up.
    int stackAt = 16 + abi_.shadowBytes;
    bool byRef = abi_.aggregatesByReference;
    for (std::size_t i = 0; i < ps.size(); i++) {
        const Type *pt = ps[i].type;
        if (byRef && pt->isStructOrUnion()) {
            // One integer slot, holding either the bytes or a pointer to them.
            bool inReg = ints + 1 <= abi_.intCount;
            std::string src;
            if (inReg) {
                src = abi_.intRegs[takeSlot(false, ints, sses)];
            } else {
                a_->ins("mov", mem(stackAt, "%rbp"), reg("%r11"));
                stackAt += 8;
                src = "%r11";
            }
            int size = pt->size(target_);
            int to = -ps[i].offset;
            if (msInRegister(size)) {
                if (src != "%rax") a_->ins("mov", reg(src), reg("%rax"));
                if (size == 8)      a_->ins("movq", reg("%rax"), mem(to, "%rbp"));
                else if (size == 4) a_->ins("movl", reg("%eax"), mem(to, "%rbp"));
                else if (size == 2) a_->ins("movw", reg("%ax"), mem(to, "%rbp"));
                else                a_->ins("movb", reg("%al"), mem(to, "%rbp"));
            } else {
                // A pointer to the caller's copy. Taking our own of it keeps
                // every later mention of the parameter an ordinary local.
                msCopyToSlot(pt, ps[i].offset, src.c_str());
            }
            continue;
        }
        // The caller put this on the stack, for the reason given where the
        // call is generated: x87 has no argument register class.
        bool memory = isX87(pt) || containsX87(pt, target_) ||
                      (pt->isStructOrUnion() &&
                       pt->size(target_) > abi_.structReturnLimit);
        if (!memory) {
            std::vector<bool> lanes;
            if (pt->isStructOrUnion()) lanes = classifyEightbytes(pt, target_);
            else                       lanes.push_back(pt->isFloating());
            int wantInt = 0, wantSse = 0;
            for (bool sse : lanes) { if (sse) wantSse++; else wantInt++; }
            memory = ints + wantInt > abi_.intCount ||
                     sses + wantSse > abi_.sseCount;
        }
        if (memory) {
            int size = pt->size(target_);
            int slots = (size + 7) / 8;
            // The caller padded up to this argument's alignment, and 16(%rbp)
            // - where the incoming arguments start - is itself sixteen-aligned,
            // so rounding the offset is the same sum the caller did.
            if (pt->align(target_) >= 16) stackAt = alignTo(stackAt, 16);
            for (int k = 0; k < slots; k++) {
                int from = stackAt + k * 8;
                int to = k * 8 - ps[i].offset;
                int left = size - k * 8;
                if (left >= 8) {
                    a_->ins("mov", mem(from, "%rbp"), reg("%rax"));
                    a_->ins("movq", reg("%rax"), mem(to, "%rbp"));
                } else if (left >= 4) {
                    a_->ins("movl", mem(from, "%rbp"), reg("%eax"));
                    a_->ins("movl", reg("%eax"), mem(to, "%rbp"));
                } else if (left >= 2) {
                    a_->ins("movzwl", mem(from, "%rbp"), reg("%eax"));
                    a_->ins("movw", reg("%ax"), mem(to, "%rbp"));
                } else {
                    a_->ins("movzbl", mem(from, "%rbp"), reg("%eax"));
                    a_->ins("movb", reg("%al"), mem(to, "%rbp"));
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
                    a_->ins(left >= 8 ? "movsd" : "movss", reg(abi_.sseRegs[takeSlot(true, ints, sses)]), mem(off, "%rbp"));
                } else {
                    a_->ins("mov", reg(abi_.intRegs[takeSlot(false, ints, sses)]), reg("%rax"));
                    if (left >= 8)      a_->ins("movq", reg("%rax"), mem(off, "%rbp"));
                    else if (left >= 4) a_->ins("movl", reg("%eax"), mem(off, "%rbp"));
                    else if (left >= 2) a_->ins("movw", reg("%ax"), mem(off, "%rbp"));
                    else                a_->ins("movb", reg("%al"), mem(off, "%rbp"));
                }
            }
            continue;
        }
        if (ps[i].type->isFloating()) {
            a_->ins("movsd", reg(abi_.sseRegs[takeSlot(true, ints, sses)]), reg("%xmm0"));
        } else {
            a_->ins("mov", reg(abi_.intRegs[takeSlot(false, ints, sses)]), reg("%rax"));
        }
        storeAt(ps[i].type, ps[i].offset);
    }
    // The walk starts where the named arguments stopped, in both files and on
    // the stack. Taken from the loop above rather than recomputed, so the two
    // cannot disagree about where the named part ended.
    varGp_ = ints * 8;
    varFp_ = 48 + sses * 16;
    // Positional slots are one per argument in either file, so the count of
    // named ones is what "ints" already holds - the two advance together.
    varOverflow_ = abi_.positional ? 16 + ints * 8 : stackAt;

    fn.body().accept(*this);

    // Falling off the end of a non-void function is undefined, but leaving the
    // x87 stack empty when the caller expects a value in st(0) is a fault
    // rather than a wrong number - so this path pushes a zero like the others.
    if (sretSlot_ != 0)                     a_->ins("mov", mem(-(sretSlot_), "%rbp"), reg("%rax"));
    else if (isX87(fn.returns()))           a_->ins("fldz");
    else if (fn.returns()->isFloating())    a_->ins("pxor", reg("%xmm0"), reg("%xmm0"));
    else                                    a_->ins("mov", immText("0"), reg("%rax"));
    a_->defLabel(returnLabel_);
    a_->ins("mov", reg("%rbp"), reg("%rsp"));
    a_->ins("pop", reg("%rbp"));
    a_->ins("ret");
    a_->functionEnd(fn.name());

    if (depth_ != 0) {
        std::fprintf(stderr, "codegen: stack depth %d at the end of %s\n",
                     depth_, fn.name().c_str());
        std::exit(1);
    }
    finishChunk();
}

void X86_64Linux::emitGlobal(const Global &g, Segment seg) {
    int size = g.type->size(target_);
    if (!g.isStatic) a_->globl(g.name);
    // What the symbol is and how much of it there is. The assembler does not
    // need either to produce correct bytes, which is why they were not here;
    // every other ELF producer emits them, and without them nm reports the
    // symbol with no size and gdb cannot print the object from its name.
    //
    // ELF only. '@object' is not COFF syntax and clang targeting PE rejects
    // the line rather than ignoring it, which is how this was found: the same
    // GNU-syntax text is assembled by gcc into ELF on Linux and by clang into
    // COFF on Windows, and only the second says anything about it.
    if (abi_.elfSymbolAttributes) {
        a_->objectType(g.name);
        a_->objectSize(g.name, size);
    }
    a_->align(objectAlign(g.type, target_));
    a_->defLabel(g.name);

    // In .bss this reserves; it does not write. The section is NOBITS, so the
    // count goes in the header and no zeroes go in the file.
    if (seg == Segment::Bss) { a_->zero(size); return; }

    int at = 0;
    for (const GlobalPiece &p : g.init) {
        if (p.offset > at) a_->zero((p.offset - at));

        // An address constant: a name for the linker to resolve, plus a byte
        // offset. Always pointer-wide, so it is a .quad whatever p.size says.
        if (!p.symbol.empty()) {
            a_->dataSym(p.symbol, p.value);
            at = p.offset + p.size;
            continue;
        }

        switch (p.size) {
        case 1: a_->dataInt(1, p.value); break;
        case 2: a_->dataInt(2, p.value); break;
        case 4: a_->dataInt(4, p.value); break;
        default: a_->dataInt(8, p.value); break;
        }
        at = p.offset + p.size;
    }
    if (at < size) a_->zero((size - at));
}

void X86_64Linux::emitData(const Program &program) {
    // A string literal is const data by definition - there is nowhere in C to
    // write through one - so it shares .rodata with the const objects.
    bool rodataOpen = false;
    if (!program.strings.empty()) {
        a_->rodataSection();
        rodataOpen = true;
        // The bytes as given, with nothing added. The terminator is already in
        // them - which is what lets a wide literal come through here unchanged,
        // its elements four bytes wide and its terminator four zeroes.
        for (const StringLit &s : program.strings) {
            a_->defLabel(s.label);
            a_->dataBytes(s.bytes);
        }
    }

    // One pass per segment, so each directive is written once and everything
    // belonging to it follows. Grouping is not decoration: an assembler that
    // is told .data forty times produces forty fragments for the linker to
    // put back together.
    struct Bucket { Segment seg; bool alreadyOpen; };
    const Bucket order[] = {
        { Segment::Const, rodataOpen },
        { Segment::Data,  false },
        { Segment::Bss,   false },
    };
    for (const Bucket &b : order) {
        bool opened = b.alreadyOpen;
        for (const Global &g : program.globals) {
            if (segmentFor(g) != b.seg) continue;
            if (!opened) {
                if (b.seg == Segment::Const)     a_->rodataSection();
                else if (b.seg == Segment::Data) a_->dataSection();
                else                             a_->bssSection();
                opened = true;
            }
            emitGlobal(g, b.seg);
        }
    }
}

void X86_64Linux::run(const Program &program) {
    // What this unit defines, told to the spelling before anything is
    // emitted. The generator knows its program; a spelling that needs the
    // list - MASM's mangling and its EXTERN block - should not have to
    // rediscover it from the text.
    std::vector<std::string> defined;
    for (const Function &fn : program.functions) defined.push_back(fn.name());
    for (const Global &g : program.globals) defined.push_back(g.name);
    for (const StringLit &s : program.strings) defined.push_back(s.label);
    a_->predefine(defined);

    emitData(program);
    finishChunk();
    for (const Function &fn : program.functions) emit(fn);

    a_->preamble(sink_);
    for (const std::string &chunk : chunks_) sink_ << chunk;
    a_->postamble(sink_);
}
