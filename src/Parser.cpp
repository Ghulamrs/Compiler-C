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

// ---- types ----

bool Parser::atTypeName() const {
    static const char *const t[] = { "void", "char", "short", "int", "long",
                                     "signed", "unsigned" };
    for (const char *k : t)
        if (peek().is(k)) return true;
    return false;
}

// Specifiers are a set, not a sequence: "unsigned long int", "long unsigned"
// and "unsigned long" are one type. They are counted and the combination is
// mapped once, so an impossible one is refused here rather than at each use.
const Type *Parser::specifiers() {
    std::size_t start = peek().pos;

    // Recognised so they can be refused clearly. Silently ignoring const would
    // let an assignment through it compile, which is worse than saying no.
    for (const char *k : { "static", "extern", "register", "const" })
        if (peek().is(k))
            src_.fail(peek().pos, std::string("'") + k + "' is not supported yet");

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
    if (isShort && isLong)
        src_.fail(start, "'short long' is not a type");
    if (isLong > 2)
        src_.fail(start, "'long long long' is not a type");

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

const Type *Parser::unsignedVersion(const Type *t) const {
    switch (t->kind()) {
    case Kind::Int:      return types_.get(Kind::UInt);
    case Kind::Long:     return types_.get(Kind::ULong);
    case Kind::LongLong: return types_.get(Kind::ULongLong);
    default:             return t;
    }
}

// The integer promotions. Anything of lower rank than int becomes int, because
// int can represent every value of every such type on all three targets.
const Type *Parser::promote(const Type *t) const {
    if (t->isInteger() && t->rank() < types_.intType()->rank())
        return types_.intType();
    return t;
}

// The usual arithmetic conversions, in the order the standard gives them. The
// last two clauses are the ones that make -1 < 1u false and, on LP64 only,
// -1L < 1u true.
const Type *Parser::usualArithmetic(const Type *a, const Type *b) const {
    a = promote(a);
    b = promote(b);
    if (a == b) return a;

    bool as = a->isSigned(target_), bs = b->isSigned(target_);
    const Type *hi = a->rank() >= b->rank() ? a : b;
    const Type *lo = (hi == a) ? b : a;

    if (as == bs) return hi;                       // same signedness: rank wins

    const Type *uns = as ? b : a;
    const Type *sig = as ? a : b;

    if (uns->rank() >= sig->rank()) return uns;    // unsigned outranks: unsigned
    if (sig->size(target_) > uns->size(target_))
        return sig;                                // signed holds every value
    return unsignedVersion(sig);                   // otherwise, unsigned of it
    (void)lo;
}

ExprPtr Parser::convert(ExprPtr e, const Type *to) const {
    if (e->type() == to) return e;
    return ExprPtr(new Cast(to, std::move(e)));
}

// ---- symbols ----

int Parser::declare(const std::string &name, const Type *type, std::size_t pos) {
    if (type->isVoid())
        src_.fail(pos, "'" + name + "' cannot have type void");
    for (const Local &l : locals_)
        if (l.name == name)
            src_.fail(pos, "'" + name + "' is declared twice");

    // Each slot takes its own size, and sits at an offset that is a multiple of
    // its own alignment - not a uniform eight bytes any more.
    frameSize_ += type->size(target_);
    frameSize_ = alignTo(frameSize_, type->align(target_));
    locals_.push_back(Local{ name, frameSize_, type });
    return frameSize_;
}

const Parser::Local &Parser::lookup(const std::string &name, std::size_t pos) const {
    for (const Local &l : locals_)
        if (l.name == name) return l;
    src_.fail(pos, "'" + name + "' was not declared");
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
                               " was declared '" + f.params[i]->name() +
                               "' and this says '" + params[i]->name() + "'");
        if (f.returns != returns)
            src_.fail(pos, "'" + name + "' was declared to return '" +
                           f.returns->name() + "' and this says '" + returns->name() + "'");
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

ExprPtr Parser::arithmetic(BinOp op, ExprPtr lhs, ExprPtr rhs) {
    const Type *common = usualArithmetic(lhs->type(), rhs->type());
    ExprPtr n(new Binary(op, convert(std::move(lhs), common),
                             convert(std::move(rhs), common)));
    n->setType(common);
    return n;
}

// A comparison converts its operands like arithmetic but is itself an int
// valued 0 or 1 - C has no boolean type here.
ExprPtr Parser::comparison(BinOp op, ExprPtr lhs, ExprPtr rhs) {
    const Type *common = usualArithmetic(lhs->type(), rhs->type());
    ExprPtr n(new Binary(op, convert(std::move(lhs), common),
                             convert(std::move(rhs), common)));
    n->setType(types_.intType());
    return n;
}

ExprPtr Parser::primary() {
    if (consume("(")) {
        ExprPtr e = expr();
        expect(")");
        return e;
    }

    if (peek().kind == TokenKind::Num) {
        const Token &t = peek();
        // The type of an integer constant: the suffix decides, and failing that
        // the smallest of int and long that holds the value.
        const Type *ty;
        if (t.suffixU && t.suffixL)      ty = types_.get(Kind::ULong);
        else if (t.suffixU)              ty = (t.value <= UINT_MAX)
                                            ? types_.get(Kind::UInt) : types_.get(Kind::ULong);
        else if (t.suffixL)              ty = types_.get(Kind::Long);
        else if (t.value >= INT_MIN && t.value <= INT_MAX)
                                         ty = types_.intType();
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
                    args.push_back(expr());
                    if (consume(")")) break;
                    expect(",");
                }
            }
            const Signature &sig = lookupFunction(name, pos);
            if (args.size() != sig.params.size())
                src_.fail(pos, "'" + name + "' takes " + std::to_string(sig.params.size()) +
                               " argument(s), given " + std::to_string(args.size()));
            // Within a prototype, arguments convert as if by assignment.
            for (std::size_t i = 0; i < args.size(); i++)
                args[i] = convert(std::move(args[i]), sig.params[i]);

            ExprPtr n(new Call(name, std::move(args)));
            n->setType(sig.returns);
            return n;
        }

        at_++;
        const Local &l = lookup(name, pos);
        ExprPtr n(new Var(name, l.offset));
        n->setType(l.type);
        return n;
    }

    src_.fail(peek().pos, "expected an expression");
}

ExprPtr Parser::unary() {
    if (consume("+")) return castExpr();
    if (consume("!")) {
        // Not promoted: the operand is only compared against zero, and the
        // answer is an int either way.
        ExprPtr node(new Unary('!', castExpr()));
        node->setType(types_.intType());
        return node;
    }
    if (consume("-")) {
        ExprPtr v = castExpr();
        const Type *t = promote(v->type());   // unary minus promotes
        ExprPtr n(new Unary('-', convert(std::move(v), t)));
        n->setType(t);
        return n;
    }
    if (peek().is("sizeof")) {
        std::size_t pos = peek().pos;
        at_++;
        const Type *measured = nullptr;
        if (peek().is("(") && [this] {
                std::size_t save = at_; at_++; bool t = atTypeName(); at_ = save; return t;
            }()) {
            at_++;                       // '('
            measured = specifiers();
            expect(")");
        } else {
            // The operand is parsed for its type and then thrown away: sizeof
            // does not evaluate what it measures.
            measured = unary()->type();
        }
        if (measured->isVoid())
            src_.fail(pos, "sizeof(void) has no meaning");
        ExprPtr n(new Num(measured->size(target_)));
        n->setType(types_.get(target_.sizeType()));
        return n;
    }
    return primary();
}

// A cast is told apart from a parenthesised expression by what follows the '('.
ExprPtr Parser::castExpr() {
    if (peek().is("(")) {
        std::size_t save = at_;
        at_++;
        if (atTypeName()) {
            const Type *to = specifiers();
            expect(")");
            ExprPtr v = castExpr();
            if (to->isVoid()) {
                ExprPtr n(new Cast(to, std::move(v)));
                return n;
            }
            return convert(std::move(v), to);
        }
        at_ = save;
    }
    return unary();
}

ExprPtr Parser::mul() {
    ExprPtr n = castExpr();
    for (;;) {
        if (consume("*"))      n = arithmetic(BinOp::Mul, std::move(n), castExpr());
        else if (consume("/")) n = arithmetic(BinOp::Div, std::move(n), castExpr());
        else if (consume("%")) n = arithmetic(BinOp::Mod, std::move(n), castExpr());
        else return n;
    }
}

ExprPtr Parser::add() {
    ExprPtr n = mul();
    for (;;) {
        if (consume("+"))      n = arithmetic(BinOp::Add, std::move(n), mul());
        else if (consume("-")) n = arithmetic(BinOp::Sub, std::move(n), mul());
        else return n;
    }
}

// The shift operators are the exception: each operand is promoted on its own
// and the result takes the type of the promoted left operand. They do not get
// the usual arithmetic conversions, so 1L << 1 is long and 1 << 1L is int.
ExprPtr Parser::shift() {
    ExprPtr n = add();
    for (;;) {
        BinOp op;
        if (consume("<<"))      op = BinOp::Shl;
        else if (consume(">>")) op = BinOp::Shr;
        else return n;

        ExprPtr rhs = add();
        const Type *lt = promote(n->type());
        ExprPtr node(new Binary(op, convert(std::move(n), lt),
                                    convert(std::move(rhs), promote(rhs->type()))));
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

// Neither operand is converted to a common type: each is only tested against
// zero, and the result is an int valued 0 or 1 whatever went in. That is why
// these do not go through arithmetic() or comparison().
ExprPtr Parser::logicalAnd() {
    ExprPtr n = equality();
    while (peek().is("&&")) {
        at_++;
        ExprPtr node(new Binary(BinOp::LAnd, std::move(n), equality()));
        node->setType(types_.intType());
        n = std::move(node);
    }
    return n;
}

ExprPtr Parser::logicalOr() {
    ExprPtr n = logicalAnd();
    while (peek().is("||")) {
        at_++;
        ExprPtr node(new Binary(BinOp::LOr, std::move(n), logicalAnd()));
        node->setType(types_.intType());
        n = std::move(node);
    }
    return n;
}

ExprPtr Parser::assign() {
    std::size_t mark = at_;
    ExprPtr n = logicalOr();
    if (!peek().is("=")) return n;

    const Token &target = tokens_[mark];
    if (target.kind != TokenKind::Ident || at_ != mark + 1)
        src_.fail(peek().pos, "left of '=' is not something that can be assigned to");
    at_++;

    const Local &l = lookup(target.text, target.pos);
    // The right side converts to the type of the left, which may lose value
    // silently - char c = 300 is implementation-defined, not an error.
    ExprPtr value = convert(assign(), l.type);
    ExprPtr node(new Assign(target.text, l.offset, std::move(value)));
    node->setType(l.type);
    return node;
}

ExprPtr Parser::expr() { return assign(); }

// ---- statements ----

StmtPtr Parser::declaration() {
    const Type *type = specifiers();
    std::size_t pos = peek().pos;
    std::string name = expectIdent("a name after the type");
    int offset = declare(name, type, pos);

    if (consume("=")) {
        ExprPtr value = convert(expr(), type);
        expect(";");
        ExprPtr a(new Assign(name, offset, std::move(value)));
        a->setType(type);
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
        body.push_back(atTypeName() ? declaration() : statement());
    }
    expect("}");
    return StmtPtr(new Block(std::move(body)));
}

StmtPtr Parser::statement() {
    if (consume("return")) {
        ExprPtr value = convert(expr(), returnType_);
        expect(";");
        return StmtPtr(new Return(std::move(value)));
    }
    if (consume("if")) {
        expect("(");
        ExprPtr cond = expr();
        expect(")");
        StmtPtr thenArm = statement();
        StmtPtr elseArm;
        if (consume("else")) elseArm = statement();
        return StmtPtr(new If(std::move(cond), std::move(thenArm), std::move(elseArm)));
    }
    if (consume("while")) {
        expect("(");
        ExprPtr cond = expr();
        expect(")");
        return StmtPtr(new While(std::move(cond), statement()));
    }
    if (peek().is("{")) return block();
    if (consume(";")) return StmtPtr(new Block({}));

    ExprPtr e = expr();
    expect(";");
    return StmtPtr(new ExprStmt(std::move(e)));
}

// ---- functions ----

void Parser::functionOrPrototype(Program &program) {
    const Type *returns = specifiers();
    std::size_t namePos = peek().pos;
    std::string name = expectIdent("a function name");
    expect("(");

    locals_.clear();
    frameSize_ = 0;
    std::vector<const Type *> params;
    std::vector<Param> paramSlots;

    if (!consume(")")) {
        if (peek().is("void") && peekAt(1).is(")")) {
            at_ += 2;
        } else {
            for (;;) {
                const Type *pt = specifiers();
                std::size_t pos = peek().pos;
                int off = declare(expectIdent("a parameter name"), pt, pos);
                params.push_back(pt);
                paramSlots.push_back(Param{ pt, off });
                if (consume(")")) break;
                expect(",");
            }
        }
    }
    if (static_cast<int>(params.size()) > kMaxArgs)
        src_.fail(namePos, "more than " + std::to_string(kMaxArgs) +
                           " parameters is not supported yet");

    if (consume(";")) {
        declareFunction(name, returns, params, false, namePos);
        return;
    }

    declareFunction(name, returns, params, true, namePos);
    returnType_ = returns;

    StmtPtr body = block();

    int frame = alignTo(frameSize_, 16);
    program.push_back(Function(std::move(name), std::move(paramSlots),
                               std::move(body), frame));
}

Program Parser::parse() {
    Program program;
    while (peek().kind != TokenKind::End)
        functionOrPrototype(program);
    if (program.empty())
        src_.fail(0, "the file defines no functions");
    return program;
}
