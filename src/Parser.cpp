#include "Parser.h"
#include "Source.h"

#include <climits>

static const int kMaxArgs = 6;   // System V's six integer argument registers

static int alignTo(int n, int a) { return (n + a - 1) / a * a; }

const Token &Parser::peekAt(std::size_t n) const {
    std::size_t i = at_ + n;
    return i < tokens_.size() ? tokens_[i] : tokens_.back();
}

bool Parser::consume(const char *s) {
    if (!peek().is(s)) return false;
    at_++;
    return true;
}

void Parser::expect(const char *s) {
    if (!peek().is(s))
        src_.fail(peek().pos, std::string("expected '") + s + "'");
    at_++;
}

std::string Parser::expectIdent(const char *what) {
    if (peek().kind != TokenKind::Ident)
        src_.fail(peek().pos, std::string("expected ") + what);
    std::string name = peek().text;
    at_++;
    return name;
}

long Parser::expectNumber(const char *what) {
    if (peek().kind != TokenKind::Num)
        src_.fail(peek().pos, std::string("expected ") + what);
    long v = peek().value;
    at_++;
    return v;
}

// ---- types ----

bool Parser::atTypeName() const {
    static const char *const t[] = { "void", "char", "short", "int", "long",
                                     "signed", "unsigned" };
    for (const char *k : t)
        if (peek().is(k)) return true;
    return false;
}

bool Parser::atDeclarationStart() const {
    return atTypeName() || peek().is("static") || peek().is("extern");
}

const Type *Parser::specifiers(StorageClass *storage) {
    std::size_t start = peek().pos;
    *storage = StorageNone;

    for (;;) {
        if (consume("static")) { *storage = StorageStatic; continue; }
        if (consume("extern")) { *storage = StorageExtern; continue; }
        if (peek().is("const") || peek().is("register"))
            src_.fail(peek().pos, "'" + peek().text + "' is not supported yet");
        break;
    }

    int isVoid = 0, isChar = 0, isShort = 0, isInt = 0, isLong = 0;
    int isSigned = 0, isUnsigned = 0;

    while (atTypeName()) {
        if (consume("void"))          isVoid++;
        else if (consume("char"))     isChar++;
        else if (consume("short"))    isShort++;
        else if (consume("int"))      isInt++;
        else if (consume("long"))     isLong++;
        else if (consume("signed"))   isSigned++;
        else if (consume("unsigned")) isUnsigned++;
    }

    if (isSigned && isUnsigned)
        src_.fail(start, "'signed' and 'unsigned' together is not a type");
    if (isVoid && (isChar || isShort || isInt || isLong || isSigned || isUnsigned))
        src_.fail(start, "'void' cannot be combined with another specifier");
    if (isChar && (isShort || isInt || isLong))
        src_.fail(start, "'char' cannot be combined with that");
    if (isShort && isLong) src_.fail(start, "'short long' is not a type");
    if (isLong > 2)        src_.fail(start, "'long long long' is not a type");

    if (isVoid)  return types_.get(Kind::Void);
    if (isChar)  return types_.get(isUnsigned ? Kind::UChar
                                  : isSigned ? Kind::SChar : Kind::Char);
    if (isShort) return types_.get(isUnsigned ? Kind::UShort : Kind::Short);
    if (isLong == 2) return types_.get(isUnsigned ? Kind::ULongLong : Kind::LongLong);
    if (isLong)  return types_.get(isUnsigned ? Kind::ULong : Kind::Long);
    if (isInt || isSigned || isUnsigned)
        return types_.get(isUnsigned ? Kind::UInt : Kind::Int);

    src_.fail(start, "expected a type");
}

// The declarator builds the type from the inside out. The array suffix binds
// tighter than the pointer prefix, which is why "int *a[10]" is an array of ten
// pointers: the '*' applies to the base first and the array wraps that.
Parser::Declared Parser::declarator(const Type *base) {
    while (consume("*")) base = types_.pointerTo(base);

    std::size_t pos = peek().pos;
    std::string name = expectIdent("a name");

    std::vector<long> dims;
    while (consume("[")) {
        dims.push_back(expectNumber("an array length"));
        expect("]");
    }
    // Applied in reverse, so a[2][3] is two of three rather than three of two.
    for (std::size_t i = dims.size(); i-- > 0; )
        base = types_.arrayOf(base, dims[i]);

    return Declared{ name, base, pos };
}

const Type *Parser::unsignedVersion(const Type *t) const {
    switch (t->kind()) {
    case Kind::Int:      return types_.get(Kind::UInt);
    case Kind::Long:     return types_.get(Kind::ULong);
    case Kind::LongLong: return types_.get(Kind::ULongLong);
    default:             return t;
    }
}

const Type *Parser::promote(const Type *t) const {
    if (t->isInteger() && t->rank() < types_.intType()->rank())
        return types_.intType();
    return t;
}

const Type *Parser::usualArithmetic(const Type *a, const Type *b) const {
    a = promote(a);
    b = promote(b);
    if (a == b) return a;

    bool as = a->isSigned(target_), bs = b->isSigned(target_);
    const Type *hi = a->rank() >= b->rank() ? a : b;
    if (as == bs) return hi;

    const Type *uns = as ? b : a;
    const Type *sig = as ? a : b;
    if (uns->rank() >= sig->rank()) return uns;
    if (sig->size(target_) > uns->size(target_)) return sig;
    return unsignedVersion(sig);
}

ExprPtr Parser::convert(ExprPtr e, const Type *to) const {
    if (e->type() == to) return e;
    return ExprPtr(new Cast(to, std::move(e)));
}

// An array used as a value is a pointer to its first element. The address is
// already what the register holds, so this changes only the type - there is no
// work for the machine to do.
ExprPtr Parser::decay(ExprPtr e) {
    if (!e->type()->isArray()) return e;
    return ExprPtr(new Cast(types_.pointerTo(e->type()->pointee()), std::move(e)));
}

void Parser::requireScalar(const Expr &e, std::size_t pos, const char *what) {
    if (!e.type()->isScalar())
        src_.fail(pos, std::string(what) + " needs a number or a pointer, not '" +
                       e.type()->describe() + "'");
}

// ---- symbols ----

int Parser::declare(const std::string &name, const Type *type, std::size_t pos) {
    if (type->isVoid())
        src_.fail(pos, "'" + name + "' cannot have type void");
    for (const Local &l : locals_)
        if (l.name == name)
            src_.fail(pos, "'" + name + "' is declared twice");

    frameSize_ += type->size(target_);
    frameSize_ = alignTo(frameSize_, type->align(target_));
    locals_.push_back(Local{ name, frameSize_, type });
    return frameSize_;
}

const Parser::Local *Parser::findLocal(const std::string &name) const {
    for (const Local &l : locals_)
        if (l.name == name) return &l;
    return nullptr;
}

const Parser::GlobalSym *Parser::findGlobal(const std::string &name) const {
    for (const GlobalSym &g : globals_)
        if (g.name == name) return &g;
    return nullptr;
}

void Parser::declareFunction(const std::string &name, const Type *returns,
                             const std::vector<const Type *> &params,
                             bool defining, std::size_t pos) {
    for (Signature &f : functions_) {
        if (f.name != name) continue;
        if (f.params.size() != params.size())
            src_.fail(pos, "'" + name + "' was declared with " +
                           std::to_string(f.params.size()) + " parameter(s), and this says " +
                           std::to_string(params.size()));
        for (std::size_t i = 0; i < params.size(); i++)
            if (f.params[i] != params[i])
                src_.fail(pos, "'" + name + "' parameter " + std::to_string(i + 1) +
                               " was declared '" + f.params[i]->describe() +
                               "' and this says '" + params[i]->describe() + "'");
        if (f.returns != returns)
            src_.fail(pos, "'" + name + "' was declared to return '" +
                           f.returns->describe() + "' and this says '" +
                           returns->describe() + "'");
        if (defining) {
            if (f.defined) src_.fail(pos, "'" + name + "' is defined twice");
            f.defined = true;
        }
        return;
    }
    functions_.push_back(Signature{ name, returns, params, defining, pos });
}

const Parser::Signature &Parser::lookupFunction(const std::string &name,
                                                std::size_t pos) const {
    for (const Signature &f : functions_)
        if (f.name == name) return f;
    src_.fail(pos, "'" + name + "' was not declared - a prototype must come first");
}

// ---- expressions ----

// p + n moves by n elements, not n bytes. Scaled here rather than in code
// generation, so the tree says what the language means.
ExprPtr Parser::pointerAdd(ExprPtr p, ExprPtr n) {
    const Type *pt = p->type();
    long stride = pt->pointee()->size(target_);

    ExprPtr size(new Num(stride));
    size->setType(types_.get(Kind::Long));
    ExprPtr scaled(new Binary(BinOp::Mul,
                              convert(std::move(n), types_.get(Kind::Long)),
                              std::move(size)));
    scaled->setType(types_.get(Kind::Long));

    ExprPtr sum(new Binary(BinOp::Add, std::move(p), std::move(scaled)));
    sum->setType(pt);
    return sum;
}

// The difference of two pointers counts elements, not bytes.
ExprPtr Parser::pointerSub(ExprPtr l, ExprPtr r, std::size_t pos) {
    if (l->type()->pointee() != r->type()->pointee())
        src_.fail(pos, "'" + l->type()->describe() + "' minus '" +
                       r->type()->describe() + "' needs the same pointee type");
    long stride = l->type()->pointee()->size(target_);

    ExprPtr diff(new Binary(BinOp::Sub, std::move(l), std::move(r)));
    diff->setType(types_.get(Kind::Long));
    ExprPtr size(new Num(stride));
    size->setType(types_.get(Kind::Long));
    ExprPtr n(new Binary(BinOp::Div, std::move(diff), std::move(size)));
    n->setType(types_.get(Kind::Long));
    return n;
}

ExprPtr Parser::arithmetic(BinOp op, ExprPtr lhs, ExprPtr rhs, std::size_t pos) {
    lhs = decay(std::move(lhs));
    rhs = decay(std::move(rhs));

    // Pointer arithmetic, before the arithmetic conversions get a chance to
    // treat an address as an ordinary number.
    if (op == BinOp::Add) {
        if (lhs->type()->isPointer() && rhs->type()->isInteger())
            return pointerAdd(std::move(lhs), std::move(rhs));
        if (lhs->type()->isInteger() && rhs->type()->isPointer())
            return pointerAdd(std::move(rhs), std::move(lhs));
    }
    if (op == BinOp::Sub && lhs->type()->isPointer()) {
        if (rhs->type()->isInteger()) {
            const Type *lt = promote(rhs->type());
            ExprPtr neg(new Unary('-', convert(std::move(rhs), lt)));
            neg->setType(lt);
            return pointerAdd(std::move(lhs), std::move(neg));
        }
        if (rhs->type()->isPointer())
            return pointerSub(std::move(lhs), std::move(rhs), pos);
    }

    if (!lhs->type()->isArithmetic() || !rhs->type()->isArithmetic())
        src_.fail(pos, "'" + lhs->type()->describe() + "' and '" +
                       rhs->type()->describe() + "' cannot be combined like that");

    const Type *common = usualArithmetic(lhs->type(), rhs->type());
    ExprPtr n(new Binary(op, convert(std::move(lhs), common),
                             convert(std::move(rhs), common)));
    n->setType(common);
    return n;
}

ExprPtr Parser::comparison(BinOp op, ExprPtr lhs, ExprPtr rhs) {
    lhs = decay(std::move(lhs));
    rhs = decay(std::move(rhs));
    ExprPtr n;
    if (lhs->type()->isPointer() || rhs->type()->isPointer()) {
        // Two addresses compare as they are; no conversions apply.
        n = ExprPtr(new Binary(op, std::move(lhs), std::move(rhs)));
    } else {
        const Type *common = usualArithmetic(lhs->type(), rhs->type());
        n = ExprPtr(new Binary(op, convert(std::move(lhs), common),
                                   convert(std::move(rhs), common)));
    }
    n->setType(types_.intType());
    return n;
}

ExprPtr Parser::primary(Program *program) {
    if (consume("(")) {
        ExprPtr e = expr();
        expect(")");
        return e;
    }

    if (peek().kind == TokenKind::Str) {
        std::string label = ".L.str." + std::to_string(strings_++);
        std::string text = peek().text;
        at_++;
        program->strings.push_back({ label, text });
        ExprPtr n(new StrLit(label, text));
        // char[N+1]: the terminating zero is part of the object, so
        // sizeof "abc" is 4.
        n->setType(types_.arrayOf(types_.charType(),
                                  static_cast<long>(text.size()) + 1));
        return n;
    }

    if (peek().kind == TokenKind::Num) {
        const Token &t = peek();
        const Type *ty;
        if (t.suffixU && t.suffixL)      ty = types_.get(Kind::ULong);
        else if (t.suffixU)              ty = (t.value <= UINT_MAX)
                                            ? types_.get(Kind::UInt) : types_.get(Kind::ULong);
        else if (t.suffixL)              ty = types_.get(Kind::Long);
        else if (t.value >= INT_MIN && t.value <= INT_MAX) ty = types_.intType();
        else                             ty = types_.get(Kind::Long);
        ExprPtr n(new Num(t.value));
        n->setType(ty);
        at_++;
        return n;
    }

    if (peek().kind == TokenKind::Ident) {
        std::string name = peek().text;
        std::size_t pos = peek().pos;

        if (peekAt(1).is("(")) {
            at_ += 2;
            std::vector<ExprPtr> args;
            if (!consume(")")) {
                for (;;) {
                    args.push_back(decay(expr()));
                    if (consume(")")) break;
                    expect(",");
                }
            }
            const Signature &sig = lookupFunction(name, pos);
            if (args.size() != sig.params.size())
                src_.fail(pos, "'" + name + "' takes " + std::to_string(sig.params.size()) +
                               " argument(s), given " + std::to_string(args.size()));
            for (std::size_t i = 0; i < args.size(); i++)
                args[i] = convert(std::move(args[i]), sig.params[i]);

            ExprPtr n(new Call(name, std::move(args)));
            n->setType(sig.returns);
            return n;
        }

        at_++;
        if (const Local *l = findLocal(name)) {
            ExprPtr n(Var::local(name, l->offset));
            n->setType(l->type);
            return n;
        }
        if (const GlobalSym *g = findGlobal(name)) {
            ExprPtr n(Var::global(name));
            n->setType(g->type);
            return n;
        }
        src_.fail(pos, "'" + name + "' was not declared");
    }

    src_.fail(peek().pos, "expected an expression");
}

// a[i] is defined as *(a + i). Building it that way rather than as its own node
// is why i[a] also works, which is legal C however strange it looks.
ExprPtr Parser::postfix() {
    ExprPtr n = primary(current_);
    while (peek().is("[")) {
        std::size_t pos = peek().pos;
        at_++;
        ExprPtr index = expr();
        expect("]");

        ExprPtr sum = arithmetic(BinOp::Add, std::move(n), std::move(index), pos);
        if (!sum->type()->isPointer())
            src_.fail(pos, "subscript needs an array or a pointer");
        const Type *elem = sum->type()->pointee();
        ExprPtr deref(new Unary('*', std::move(sum)));
        deref->setType(elem);
        n = std::move(deref);
    }
    return n;
}

ExprPtr Parser::unary() {
    std::size_t pos = peek().pos;

    if (consume("+")) return decay(castExpr());

    if (consume("!")) {
        ExprPtr v = decay(castExpr());
        requireScalar(*v, pos, "'!'");
        ExprPtr node(new Unary('!', std::move(v)));
        node->setType(types_.intType());
        return node;
    }
    if (consume("-")) {
        ExprPtr v = decay(castExpr());
        if (!v->type()->isArithmetic())
            src_.fail(pos, "unary '-' needs a number, not '" + v->type()->describe() + "'");
        const Type *t = promote(v->type());
        ExprPtr n(new Unary('-', convert(std::move(v), t)));
        n->setType(t);
        return n;
    }
    if (consume("&")) {
        // Deliberately not decayed: &a where a is char[16] is a pointer to the
        // array, not to its first element.
        ExprPtr v = castExpr();
        const Type *of = v->type();
        ExprPtr n(new Unary('&', std::move(v)));
        n->setType(types_.pointerTo(of));
        return n;
    }
    if (consume("*")) {
        ExprPtr v = decay(castExpr());
        if (!v->type()->isPointer())
            src_.fail(pos, "'*' needs a pointer, not '" + v->type()->describe() + "'");
        const Type *elem = v->type()->pointee();
        if (elem->isVoid()) src_.fail(pos, "'void *' cannot be dereferenced");
        ExprPtr n(new Unary('*', std::move(v)));
        n->setType(elem);
        return n;
    }
    if (peek().is("sizeof")) {
        at_++;
        const Type *measured = nullptr;
        if (peek().is("(") && [this] {
                std::size_t save = at_; at_++; bool t = atTypeName(); at_ = save; return t;
            }()) {
            at_++;
            StorageClass sc;
            measured = specifiers(&sc);
            while (consume("*")) measured = types_.pointerTo(measured);
            expect(")");
        } else {
            // Not decayed: sizeof of an array is the array's own size, which is
            // the whole reason decay has exceptions.
            measured = unary()->type();
        }
        if (!measured->isComplete())
            src_.fail(pos, "sizeof needs a complete type");
        ExprPtr n(new Num(measured->size(target_)));
        n->setType(types_.get(target_.sizeType()));
        return n;
    }
    return postfix();
}

ExprPtr Parser::castExpr() {
    if (peek().is("(")) {
        std::size_t save = at_;
        at_++;
        if (atTypeName()) {
            StorageClass sc;
            const Type *to = specifiers(&sc);
            while (consume("*")) to = types_.pointerTo(to);
            expect(")");
            ExprPtr v = decay(castExpr());
            if (to->isVoid()) return ExprPtr(new Cast(to, std::move(v)));
            return convert(std::move(v), to);
        }
        at_ = save;
    }
    return unary();
}

ExprPtr Parser::mul() {
    ExprPtr n = castExpr();
    for (;;) {
        std::size_t pos = peek().pos;
        if (consume("*"))      n = arithmetic(BinOp::Mul, std::move(n), castExpr(), pos);
        else if (consume("/")) n = arithmetic(BinOp::Div, std::move(n), castExpr(), pos);
        else if (consume("%")) n = arithmetic(BinOp::Mod, std::move(n), castExpr(), pos);
        else return n;
    }
}

ExprPtr Parser::add() {
    ExprPtr n = mul();
    for (;;) {
        std::size_t pos = peek().pos;
        if (consume("+"))      n = arithmetic(BinOp::Add, std::move(n), mul(), pos);
        else if (consume("-")) n = arithmetic(BinOp::Sub, std::move(n), mul(), pos);
        else return n;
    }
}

ExprPtr Parser::shift() {
    ExprPtr n = add();
    for (;;) {
        BinOp op;
        if (consume("<<"))      op = BinOp::Shl;
        else if (consume(">>")) op = BinOp::Shr;
        else return n;

        ExprPtr r = add();
        const Type *lt = promote(n->type());
        ExprPtr node(new Binary(op, convert(std::move(n), lt),
                                    convert(std::move(r), promote(r->type()))));
        node->setType(lt);
        n = std::move(node);
    }
}

ExprPtr Parser::relational() {
    ExprPtr n = shift();
    for (;;) {
        if (consume("<"))       n = comparison(BinOp::Lt, std::move(n), shift());
        else if (consume("<=")) n = comparison(BinOp::Le, std::move(n), shift());
        else if (consume(">"))  n = comparison(BinOp::Gt, std::move(n), shift());
        else if (consume(">=")) n = comparison(BinOp::Ge, std::move(n), shift());
        else return n;
    }
}

ExprPtr Parser::equality() {
    ExprPtr n = relational();
    for (;;) {
        if (consume("=="))      n = comparison(BinOp::Eq, std::move(n), relational());
        else if (consume("!=")) n = comparison(BinOp::Ne, std::move(n), relational());
        else return n;
    }
}

ExprPtr Parser::logicalAnd() {
    ExprPtr n = equality();
    while (peek().is("&&")) {
        std::size_t pos = peek().pos;
        at_++;
        ExprPtr r = decay(equality());
        n = decay(std::move(n));
        requireScalar(*n, pos, "'&&'");
        requireScalar(*r, pos, "'&&'");
        ExprPtr node(new Binary(BinOp::LAnd, std::move(n), std::move(r)));
        node->setType(types_.intType());
        n = std::move(node);
    }
    return n;
}

ExprPtr Parser::logicalOr() {
    ExprPtr n = logicalAnd();
    while (peek().is("||")) {
        std::size_t pos = peek().pos;
        at_++;
        ExprPtr r = decay(logicalAnd());
        n = decay(std::move(n));
        requireScalar(*n, pos, "'||'");
        requireScalar(*r, pos, "'||'");
        ExprPtr node(new Binary(BinOp::LOr, std::move(n), std::move(r)));
        node->setType(types_.intType());
        n = std::move(node);
    }
    return n;
}

// An lvalue is a name or a dereference. A subscript is a dereference by
// construction, so it needs no case of its own.
static bool isLvalue(const Expr &e) {
    if (dynamic_cast<const Var *>(&e)) return true;
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) return u->op() == '*';
    return false;
}

ExprPtr Parser::assign() {
    ExprPtr n = logicalOr();
    if (!peek().is("=")) return n;
    std::size_t pos = peek().pos;
    at_++;

    if (!isLvalue(*n))
        src_.fail(pos, "left of '=' is not something that can be assigned to");
    if (n->type()->isArray())
        src_.fail(pos, "an array cannot be assigned to");

    const Type *to = n->type();
    ExprPtr value = convert(decay(assign()), to);
    ExprPtr node(new Assign(std::move(n), std::move(value)));
    node->setType(to);
    return node;
}

ExprPtr Parser::expr() { return assign(); }

// ---- statements ----

StmtPtr Parser::declaration() {
    StorageClass sc;
    const Type *base = specifiers(&sc);
    if (sc != StorageNone)
        src_.fail(peek().pos, "a storage class on a local is not supported yet");

    Declared d = declarator(base);
    int offset = declare(d.name, d.type, d.pos);

    if (consume("=")) {
        if (d.type->isArray())
            src_.fail(d.pos, "an array initialiser is not supported yet");
        ExprPtr value = convert(decay(expr()), d.type);
        expect(";");
        ExprPtr target(Var::local(d.name, offset));
        target->setType(d.type);
        ExprPtr a(new Assign(std::move(target), std::move(value)));
        a->setType(d.type);
        return StmtPtr(new ExprStmt(std::move(a)));
    }
    expect(";");
    return StmtPtr(new Block({}));
}

StmtPtr Parser::block() {
    expect("{");
    std::vector<StmtPtr> body;
    while (!peek().is("}")) {
        if (peek().kind == TokenKind::End)
            src_.fail(peek().pos, "unclosed '{'");
        body.push_back(atDeclarationStart() ? declaration() : statement());
    }
    expect("}");
    return StmtPtr(new Block(std::move(body)));
}

StmtPtr Parser::statement() {
    if (consume("return")) {
        ExprPtr value = convert(decay(expr()), returnType_);
        expect(";");
        return StmtPtr(new Return(std::move(value)));
    }
    if (consume("if")) {
        expect("(");
        ExprPtr cond = decay(expr());
        expect(")");
        StmtPtr thenArm = statement();
        StmtPtr elseArm;
        if (consume("else")) elseArm = statement();
        return StmtPtr(new If(std::move(cond), std::move(thenArm), std::move(elseArm)));
    }
    if (consume("while")) {
        expect("(");
        ExprPtr cond = decay(expr());
        expect(")");
        return StmtPtr(new While(std::move(cond), statement()));
    }
    if (peek().is("{")) return block();
    if (consume(";")) return StmtPtr(new Block({}));

    ExprPtr e = expr();
    expect(";");
    return StmtPtr(new ExprStmt(std::move(e)));
}

// ---- the top level ----

void Parser::topLevel(Program &program) {
    StorageClass sc;
    const Type *base = specifiers(&sc);

    // The declarator has to be read before it is known whether this declares a
    // function or an object: "int *f(void)" and "int *p" begin identically.
    locals_.clear();
    frameSize_ = 0;
    Declared d = declarator(base);

    if (!peek().is("(")) {
        if (d.type->isVoid()) src_.fail(d.pos, "'" + d.name + "' cannot have type void");
        if (findGlobal(d.name)) src_.fail(d.pos, "'" + d.name + "' is declared twice");

        long init = 0;
        bool hasInit = false;
        if (consume("=")) {
            if (d.type->isArray())
                src_.fail(d.pos, "an array initialiser is not supported yet");
            // An integer constant only, so no constant-expression evaluator is
            // needed yet.
            bool neg = consume("-");
            init = expectNumber("a constant initialiser");
            if (neg) init = -init;
            hasInit = true;
        }
        expect(";");
        globals_.push_back(GlobalSym{ d.name, d.type });
        // extern says the object lives in another unit, so nothing is emitted.
        if (sc != StorageExtern)
            program.globals.push_back(Global{ d.name, d.type, init, hasInit,
                                              sc == StorageStatic });
        return;
    }

    expect("(");
    std::vector<const Type *> params;
    std::vector<Param> paramSlots;

    if (!consume(")")) {
        if (peek().is("void") && peekAt(1).is(")")) {
            at_ += 2;
        } else {
            for (;;) {
                StorageClass psc;
                const Type *pt = specifiers(&psc);
                Declared pd = declarator(pt);
                // A parameter declared as an array is a pointer. That rule is
                // why sizeof inside a function gives 8 for a char[16] parameter.
                if (pd.type->isArray())
                    pd.type = types_.pointerTo(pd.type->pointee());
                int off = declare(pd.name, pd.type, pd.pos);
                params.push_back(pd.type);
                paramSlots.push_back(Param{ pd.type, off });
                if (consume(")")) break;
                expect(",");
            }
        }
    }
    if (static_cast<int>(params.size()) > kMaxArgs)
        src_.fail(d.pos, "more than " + std::to_string(kMaxArgs) +
                         " parameters is not supported yet");

    if (consume(";")) {
        declareFunction(d.name, d.type, params, false, d.pos);
        return;
    }

    declareFunction(d.name, d.type, params, true, d.pos);
    returnType_ = d.type;

    StmtPtr body = block();

    int frame = alignTo(frameSize_, 16);
    program.functions.push_back(Function(d.name, std::move(paramSlots),
                                         std::move(body), frame));
}

Program Parser::parse() {
    Program program;
    current_ = &program;
    while (peek().kind != TokenKind::End)
        topLevel(program);
    if (program.functions.empty())
        src_.fail(0, "the file defines no functions");
    return program;
}
