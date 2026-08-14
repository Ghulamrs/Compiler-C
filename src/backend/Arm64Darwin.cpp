#include "Arm64Darwin.h"

#include <cstdio>
#include <cstdlib>
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

void Arm64Darwin::movImm(const char *reg, long value) {
    unsigned long u = static_cast<unsigned long>(value);
    out_ << "  mov " << reg << ", #" << (u & 0xffff) << "\n";
    for (int shift = 16; shift < 64; shift += 16) {
        unsigned long part = (u >> shift) & 0xffff;
        if (part != 0) out_ << "  movk " << reg << ", #" << part
                            << ", lsl #" << shift << "\n";
    }
}

void Arm64Darwin::genAddr(const Expr &e) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        if (v->isLocal()) {
            // Computed rather than folded: a frame can outgrow the offset field.
            movImm("x9", v->offset());
            out_ << "  sub x0, x29, x9\n";
        } else {
            out_ << "  adrp x0, _" << v->name() << "@PAGE\n";
            out_ << "  add x0, x0, _" << v->name() << "@PAGEOFF\n";
        }
        return;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') { u->operand().accept(*this); return; }
    }
    if (const StrLit *s = dynamic_cast<const StrLit *>(&e)) {
        out_ << "  adrp x0, " << s->label() << "@PAGE\n";
        out_ << "  add x0, x0, " << s->label() << "@PAGEOFF\n";
        return;
    }
    unsupported("the address of this expression");
}

void Arm64Darwin::load(const Type *t) {
    if (t->isArray() || t->isStructOrUnion()) return;
    if (t->isFloating()) unsupported("floating point");

    int sz = t->size(target_);
    bool sign = t->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  ldrsb x0, [x0]\n" : "  ldrb w0, [x0]\n");
    else if (sz == 2) out_ << (sign ? "  ldrsh x0, [x0]\n" : "  ldrh w0, [x0]\n");
    else if (sz == 4) out_ << (sign ? "  ldrsw x0, [x0]\n" : "  ldr w0, [x0]\n");
    else              out_ << "  ldr x0, [x0]\n";
}

void Arm64Darwin::storeThrough(const Type *t, const char *addrReg) {
    if (t->isFloating()) unsupported("floating point");
    switch (t->size(target_)) {
    case 1:  out_ << "  strb w0, [" << addrReg << "]\n"; return;
    case 2:  out_ << "  strh w0, [" << addrReg << "]\n"; return;
    case 4:  out_ << "  str w0, [" << addrReg << "]\n"; return;
    default: out_ << "  str x0, [" << addrReg << "]\n"; return;
    }
}

void Arm64Darwin::visit(const Num &n) {
    if (n.type()->isFloating()) unsupported("floating point");
    movImm("x0", n.value());
}

void Arm64Darwin::visit(const Var &n) { genAddr(n); load(n.type()); }

void Arm64Darwin::visit(const VaStart &) { unsupported("va_start"); }

void Arm64Darwin::visit(const StrLit &n) { genAddr(n); }

void Arm64Darwin::visit(const MemberAccess &) { unsupported("struct members"); }

void Arm64Darwin::visit(const Assign &n) {
    genAddr(n.target());
    push();
    n.value().accept(*this);
    pop("x1");
    storeThrough(n.type(), "x1");
}

void Arm64Darwin::visit(const Cast &n) {
    n.value().accept(*this);
    const Type *to = n.type();
    if (to->isVoid()) return;
    if (to->isFloating() || n.value().type()->isFloating())
        unsupported("floating point");

    int sz = to->size(target_);
    bool sign = to->isSigned(target_);
    if (sz == 1)      out_ << (sign ? "  sxtb x0, w0\n" : "  uxtb w0, w0\n");
    else if (sz == 2) out_ << (sign ? "  sxth x0, w0\n" : "  uxth w0, w0\n");
    else if (sz == 4) out_ << (sign ? "  sxtw x0, w0\n" : "  mov w0, w0\n");
}

void Arm64Darwin::genTruth(const Expr &e) {
    e.accept(*this);
    out_ << "  cmp x0, #0\n";
    out_ << "  cset x0, ne\n";
}

void Arm64Darwin::visit(const Unary &n) {
    switch (n.op()) {
    case '-':
        n.operand().accept(*this);
        out_ << "  neg x0, x0\n";
        return;
    case '~':
        n.operand().accept(*this);
        out_ << "  mvn x0, x0\n";
        return;
    case '!':
        n.operand().accept(*this);
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

    if (n.lhs().type()->isFloating() || n.rhs().type()->isFloating())
        unsupported("floating point");

    n.lhs().accept(*this);
    push();
    n.rhs().accept(*this);
    out_ << "  mov x1, x0\n";
    pop("x0");

    bool sign = n.lhs().type()->isSigned(target_);

    switch (n.op()) {
    case BinOp::Add: out_ << "  add x0, x0, x1\n"; return;
    case BinOp::Sub: out_ << "  sub x0, x0, x1\n"; return;
    case BinOp::Mul: out_ << "  mul x0, x0, x1\n"; return;
    case BinOp::Div:
        out_ << (sign ? "  sdiv x0, x0, x1\n" : "  udiv x0, x0, x1\n");
        return;
    case BinOp::Mod:
        out_ << (sign ? "  sdiv x2, x0, x1\n" : "  udiv x2, x0, x1\n");
        out_ << "  msub x0, x2, x1, x0\n";
        return;
    case BinOp::BitAnd: out_ << "  and x0, x0, x1\n"; return;
    case BinOp::BitOr:  out_ << "  orr x0, x0, x1\n"; return;
    case BinOp::BitXor: out_ << "  eor x0, x0, x1\n"; return;
    case BinOp::Shl:    out_ << "  lsl x0, x0, x1\n"; return;
    case BinOp::Shr:
        out_ << (sign ? "  asr x0, x0, x1\n" : "  lsr x0, x0, x1\n");
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

void Arm64Darwin::visit(const Postfix &) { unsupported("postfix ++ and --"); }

void Arm64Darwin::visit(const Call &n) {
    if (n.callee() != nullptr) unsupported("calls through a function pointer");
    if (n.type()->isStructOrUnion()) unsupported("a struct returned by value");

    const std::vector<ExprPtr> &args = n.args();
    std::size_t named = static_cast<std::size_t>(n.namedArgs());
    if (named > args.size()) named = args.size();
    std::size_t extra = args.size() - named;

    if (named > static_cast<std::size_t>(abi_.intCount))
        unsupported("more named arguments than the registers hold");
    for (const ExprPtr &a : args)
        if (a->type()->isFloating() || a->type()->isStructOrUnion())
            unsupported("floating or aggregate arguments");

    // Apple's deviation from AAPCS64: the variadic part goes on the stack in
    // eight-byte slots, never in registers. Follow the standard here and printf
    // reads whatever was lying in x0-x7.
    int extraBytes = alignTo(static_cast<int>(extra) * 8, 16);
    if (extraBytes > 0) {
        movImm("x9", extraBytes);
        out_ << "  sub sp, sp, x9\n";
        for (std::size_t k = 0; k < extra; k++) {
            args[named + k]->accept(*this);
            out_ << "  str x0, [sp, #" << (k * 8) << "]\n";
        }
    }

    for (std::size_t i = 0; i < named; i++) {
        args[i]->accept(*this);
        push();
    }
    for (std::size_t i = named; i-- > 0; ) pop(abi_.intRegs[i]);

    out_ << "  bl _" << n.name() << "\n";
    if (extraBytes > 0) {
        movImm("x9", extraBytes);
        out_ << "  add sp, sp, x9\n";
    }
    if (!n.type()->isVoid() && n.type()->size(target_) == 4 &&
        n.type()->isSigned(target_))
        out_ << "  sxtw x0, w0\n";
}

void Arm64Darwin::visit(const ExprStmt &n) { n.expr().accept(*this); }

void Arm64Darwin::visit(const Return &n) {
    n.value().accept(*this);
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

void Arm64Darwin::visit(const Continue &) {
    out_ << "  b " << jumps_.back().cont << "\n";
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

void Arm64Darwin::visit(const Switch &) { unsupported("switch"); }
void Arm64Darwin::visit(const Case &)   { unsupported("case"); }

void Arm64Darwin::visit(const Goto &n) {
    out_ << "  b " << userLabel(n.label()) << "\n";
}

void Arm64Darwin::visit(const Label &n) {
    out_ << userLabel(n.name()) << ":\n";
    n.body().accept(*this);
}

void Arm64Darwin::emitData(const Program &program) {
    for (const std::pair<std::string, std::string> &s : program.strings) {
        out_ << "  .section __TEXT,__cstring,cstring_literals\n";
        out_ << s.first << ":\n";
        out_ << "  .asciz \"";
        for (unsigned char c : s.second) {
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

    for (const Global &g : program.globals) {
        int size = g.type->size(target_);
        out_ << "  .section __DATA,__data\n";
        if (!g.isStatic) out_ << "  .globl _" << g.name << "\n";
        out_ << "  .p2align 3\n";
        out_ << "_" << g.name << ":\n";
        if (!g.hasInit) {
            out_ << "  .space " << size << "\n";
            continue;
        }
        int at = 0;
        for (const GlobalPiece &p : g.init) {
            if (p.offset > at) out_ << "  .space " << (p.offset - at) << "\n";
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
}

void Arm64Darwin::emitFunction(const Function &fn) {
    labels_ = 0;
    functionName_ = fn.name();
    labelPrefix_ = "L." + fn.name() + ".";
    returnLabel_ = "L.return." + fn.name();

    if (fn.returns()->isStructOrUnion()) unsupported("a struct returned by value");

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

    const std::vector<Param> &ps = fn.params();
    if (ps.size() > static_cast<std::size_t>(abi_.intCount))
        unsupported("more parameters than the registers hold");
    for (std::size_t i = 0; i < ps.size(); i++) {
        if (ps[i].type->isFloating() || ps[i].type->isStructOrUnion())
            unsupported("floating or aggregate parameters");
        movImm("x9", ps[i].offset);
        out_ << "  sub x9, x29, x9\n";
        out_ << "  mov x0, " << abi_.intRegs[i] << "\n";
        storeThrough(ps[i].type, "x9");
    }

    fn.body().accept(*this);

    out_ << "  mov x0, #0\n";
    out_ << returnLabel_ << ":\n";
    out_ << "  mov sp, x29\n";
    out_ << "  ldp x29, x30, [sp], #16\n";
    out_ << "  ret\n";
}

void Arm64Darwin::run(const Program &program) {
    emitData(program);
    for (const Function &fn : program.functions) emitFunction(fn);
    out_ << ".subsections_via_symbols\n";
    sink_ << out_.str();
}
