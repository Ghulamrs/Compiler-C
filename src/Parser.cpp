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

const Type *Parser::findTypedef(const std::string &name) const {
    auto it = typedefIndex_.find(name);
    return it == typedefIndex_.end() ? nullptr : typedefs_[it->second].type;
}

const Parser::EnumConst *Parser::findEnum(const std::string &name) const {
    auto it = enumIndex_.find(name);
    return it == enumIndex_.end() ? nullptr : &enums_[it->second];
}

// Whether a type starts here. The last clause is the whole of C's most famous
// ambiguity: "(A)*b" is a cast when A is a typedef name and a multiplication
// when it is a variable, and no amount of grammar decides it. Only this table
// does, which is why it lives in the parser and is consulted while parsing.
bool Parser::atTypeName() const {
    static const char *const t[] = { "void", "char", "short", "int", "long",
                                     "signed", "unsigned", "float", "double",
                                     "struct", "union", "enum" };
    for (const char *k : t)
        if (peek().is(k)) return true;
    return peek().kind == TokenKind::Ident && findTypedef(peek().text) != nullptr;
}

bool Parser::atDeclarationStart() const {
    return atTypeName() || peek().is("static") || peek().is("extern");
}

// struct and union differ in one line of layout and nothing else: a union puts
// every member at zero. Both round their size up to their own alignment, so an
// array of them stays aligned - which is why sizeof can exceed the sum of the
// members.
const Type *Parser::structOrUnionSpecifier(Kind kind) {
    const char *what = kind == Kind::Struct ? "struct" : "union";
    std::size_t pos = peek().pos;

    std::string tag;
    if (peek().kind == TokenKind::Ident) { tag = peek().text; at_++; }

    Type *type = tag.empty() ? types_.anonymousStruct(kind)
                             : types_.structType(kind, tag);

    if (!peek().is("{")) {
        // A mention rather than a definition. "struct Node *next;" inside
        // struct Node works because the type exists here before it is complete.
        if (tag.empty()) src_.fail(pos, std::string(what) + " needs a tag or a body");
        return type;
    }
    at_++;

    if (type->isComplete())
        src_.fail(pos, std::string(what) + " " + tag + " is defined twice");

    std::vector<Member> members;
    int offset = 0, widest = 1;
    while (!peek().is("}")) {
        if (peek().kind == TokenKind::End) src_.fail(pos, "unclosed '{'");
        StorageClass msc;
        const Type *base = specifiers(&msc);
        if (msc != StorageNone)
            src_.fail(peek().pos, "a storage class on a member is not supported yet");
        for (;;) {
            Declared d = declarator(base);
            if (!d.type->isComplete())
                src_.fail(d.pos, "'" + d.name + "' has an incomplete type");
            int a = d.type->align(target_);
            if (a > widest) widest = a;
            // A union stacks every member at zero; a struct places each at the
            // next offset that is a multiple of its own alignment.
            int at = (kind == Kind::Union) ? 0 : alignTo(offset, a);
            members.push_back(Member{ d.name, d.type, at });
            int end = at + d.type->size(target_);
            if (kind == Kind::Union) { if (end > offset) offset = end; }
            else offset = end;
            if (!consume(",")) break;
        }
        expect(";");
    }
    expect("}");

    if (members.empty()) src_.fail(pos, std::string(what) + " has no members");
    type->complete(members, alignTo(offset, widest), widest);
    return type;
}

// enum is an alias for int here. The enumerators are what matter, and they are
// int constants; a distinct type would buy no checking this compiler performs.
const Type *Parser::enumSpecifier() {
    std::size_t pos = peek().pos;
    if (peek().kind == TokenKind::Ident) at_++;   // the tag, unused

    if (!peek().is("{")) return types_.intType();
    at_++;

    long next = 0;
    while (!peek().is("}")) {
        std::size_t npos = peek().pos;
        std::string name = expectIdent("an enumerator");
        if (findEnum(name)) src_.fail(npos, "'" + name + "' is declared twice");
        if (consume("=")) {
            bool neg = consume("-");
            next = expectNumber("a constant");
            if (neg) next = -next;
        }
        enumIndex_[name] = enums_.size();
        enums_.push_back(EnumConst{ name, next });
        next = next + 1;
        if (!consume(",")) break;
    }
    expect("}");
    if (enums_.empty()) src_.fail(pos, "enum has no enumerators");
    return types_.intType();
}

const Type *Parser::specifiers(StorageClass *storage) {
    std::size_t start = peek().pos;
    *storage = StorageNone;

    for (;;) {
        if (consume("static"))  { *storage = StorageStatic; continue; }
        if (consume("extern"))  { *storage = StorageExtern; continue; }
        if (consume("typedef")) { *storage = StorageTypedef; continue; }
        if (peek().is("const") || peek().is("register"))
            src_.fail(peek().pos, "'" + peek().text + "' is not supported yet");
        break;
    }

    // These three produce a type outright rather than contributing to a set.
    if (peek().is("struct")) { at_++; return structOrUnionSpecifier(Kind::Struct); }
    if (peek().is("union"))  { at_++; return structOrUnionSpecifier(Kind::Union); }
    if (peek().is("enum"))   { at_++; return enumSpecifier(); }
    if (peek().kind == TokenKind::Ident) {
        if (const Type *t = findTypedef(peek().text)) { at_++; return t; }
    }

    int isVoid = 0, isChar = 0, isShort = 0, isInt = 0, isLong = 0;
    int isSigned = 0, isUnsigned = 0, isFloat = 0, isDouble = 0;

    while (atTypeName()) {
        if (consume("float"))         isFloat++;
        else if (consume("double"))   isDouble++;
        else if (consume("void"))     isVoid++;
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
    if ((isFloat || isDouble) && (isChar || isShort || isInt || isSigned || isUnsigned))
        src_.fail(start, "a floating type cannot be combined with that");
    if (isFloat && isDouble)
        src_.fail(start, "'float double' is not a type");
    if (isDouble && isLong)
        src_.fail(start, "'long double' is not supported yet");

    if (isFloat)  return types_.get(Kind::Float);
    if (isDouble) return types_.get(Kind::Double);
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
    // Floating wins outright, and double beats float. These three clauses come
    // before the integer rules and before promotion, which only concerns
    // integers.
    if (a->kind() == Kind::Double || b->kind() == Kind::Double)
        return types_.doubleType();
    if (a->kind() == Kind::Float || b->kind() == Kind::Float)
        return types_.get(Kind::Float);

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

void Parser::enterScope() { scopeStarts_.push_back(locals_.size()); }

void Parser::leaveScope() {
    locals_.resize(scopeStarts_.back());
    scopeStarts_.pop_back();
}

int Parser::declare(const std::string &name, const Type *type, std::size_t pos) {
    if (type->isVoid())
        src_.fail(pos, "'" + name + "' cannot have type void");
    // Only within this block. An inner declaration of the same name shadows
    // the outer one, which is what C says and what "for (int i" twice needs.
    std::size_t from = scopeStarts_.empty() ? 0 : scopeStarts_.back();
    for (std::size_t i = from; i < locals_.size(); i++)
        if (locals_[i].name == name)
            src_.fail(pos, "'" + name + "' is declared twice in this block");

    frameSize_ += type->size(target_);
    frameSize_ = alignTo(frameSize_, type->align(target_));
    locals_.push_back(Local{ name, frameSize_, type });
    return frameSize_;
}

// Innermost outwards, so the nearest declaration wins.
const Parser::Local *Parser::findLocal(const std::string &name) const {
    for (std::size_t i = locals_.size(); i-- > 0; )
        if (locals_[i].name == name) return &locals_[i];
    return nullptr;
}

const Parser::GlobalSym *Parser::findGlobal(const std::string &name) const {
    auto it = globalIndex_.find(name);
    return it == globalIndex_.end() ? nullptr : &globals_[it->second];
}

ExprPtr Parser::defaultPromote(ExprPtr e) {
    if (e->type()->kind() == Kind::Float)
        return convert(std::move(e), types_.doubleType());
    if (e->type()->isInteger())
        return convert(std::move(e), promote(e->type()));
    return e;
}

void Parser::declareFunction(const std::string &name, const Type *returns,
                             const std::vector<const Type *> &params,
                             bool variadic, bool defining, std::size_t pos) {
    auto known = functionIndex_.find(name);
    if (known != functionIndex_.end()) {
        Signature &f = functions_[known->second];
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
    functionIndex_[name] = functions_.size();
    functions_.push_back(Signature{ name, returns, params, variadic, defining, pos });
}

const Parser::Signature &Parser::lookupFunction(const std::string &name,
                                                std::size_t pos) const {
    auto it = functionIndex_.find(name);
    if (it != functionIndex_.end()) return functions_[it->second];
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
    if (op == BinOp::Mod && (lhs->type()->isFloating() || rhs->type()->isFloating()))
        src_.fail(pos, "'%' needs integers, not floating point");

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

    if (peek().kind == TokenKind::Num && peek().isFloat) {
        const Token &t = peek();
        ExprPtr n(new Num(t.dvalue));
        // 1.5 is a double; only the f suffix makes it a float.
        n->setType(types_.get(t.suffixF ? Kind::Float : Kind::Double));
        at_++;
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
            if (sig.variadic ? args.size() < sig.params.size()
                             : args.size() != sig.params.size())
                src_.fail(pos, "'" + name + "' takes " +
                               (sig.variadic ? "at least " : "") +
                               std::to_string(sig.params.size()) +
                               " argument(s), given " + std::to_string(args.size()));
            // Named parameters convert as if by assignment; the rest take the
            // default argument promotions, which is a different rule.
            for (std::size_t i = 0; i < args.size(); i++)
                args[i] = i < sig.params.size()
                        ? convert(std::move(args[i]), sig.params[i])
                        : defaultPromote(std::move(args[i]));

            // The register limit is System V's, not the parser's, but it is
            // caught here because here there is a line to point at. Code
            // generation could only say that something, somewhere, had too
            // many arguments.
            int ints = 0, sses = 0;
            for (const ExprPtr &a : args) {
                if (a->type()->isFloating()) sses++; else ints++;
            }
            if (ints > kMaxArgs)
                src_.fail(pos, "'" + name + "' is called with " + std::to_string(ints) +
                               " integer arguments; only " + std::to_string(kMaxArgs) +
                               " fit in registers and the rest would go on the stack, "
                               "which is not supported yet");
            if (sses > 8)
                src_.fail(pos, "'" + name + "' is called with " + std::to_string(sses) +
                               " floating arguments; only 8 fit in registers");

            ExprPtr n(new Call(name, std::move(args), sig.variadic));
            n->setType(sig.returns);
            return n;
        }

        at_++;
        if (const EnumConst *e = findEnum(name)) {
            ExprPtr n(new Num(e->value));
            n->setType(types_.intType());
            return n;
        }
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
    for (;;) {
        std::size_t pos = peek().pos;

        if (peek().is("[")) {
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
            continue;
        }

        // p->m is (*p).m. Lowering it here rather than giving it a node means
        // there is one path to a member and one place to get the offset right.
        if (peek().is("->")) {
            at_++;
            if (!n->type()->isPointer() || !n->type()->pointee()->isStructOrUnion())
                src_.fail(pos, "'->' needs a pointer to a struct or union, not '" +
                               n->type()->describe() + "'");
            const Type *obj = n->type()->pointee();
            ExprPtr deref(new Unary('*', std::move(n)));
            deref->setType(obj);
            n = std::move(deref);
            // fall through to the '.' handling below
            std::string name = expectIdent("a member name");
            const Member *m = obj->findMember(name);
            if (!m) src_.fail(pos, "'" + obj->describe() + "' has no member '" + name + "'");
            ExprPtr acc(new MemberAccess(std::move(n), name, m->offset));
            acc->setType(m->type);
            n = std::move(acc);
            continue;
        }

        if (peek().is("++") || peek().is("--")) {
            src_.fail(pos, "postfix '++' and '--' are not supported yet - "
                           "the old value needs a temporary this compiler cannot make; "
                           "write the prefix form where the difference does not matter");
        }

        if (peek().is(".")) {
            at_++;
            if (!n->type()->isStructOrUnion())
                src_.fail(pos, "'.' needs a struct or union, not '" +
                               n->type()->describe() + "'");
            const Type *obj = n->type();
            std::string name = expectIdent("a member name");
            const Member *m = obj->findMember(name);
            if (!m) src_.fail(pos, "'" + obj->describe() + "' has no member '" + name + "'");
            ExprPtr acc(new MemberAccess(std::move(n), name, m->offset));
            acc->setType(m->type);
            n = std::move(acc);
            continue;
        }

        return n;
    }
}

ExprPtr Parser::unary() {
    std::size_t pos = peek().pos;

    if (consume("+")) return decay(castExpr());

    if (peek().is("++") || peek().is("--")) {
        bool inc = peek().is("++");
        at_++;
        return incDec(unary(), inc, true, pos);
    }
    if (consume("~")) {
        ExprPtr v = decay(castExpr());
        if (!v->type()->isInteger())
            src_.fail(pos, "'~' needs an integer, not '" + v->type()->describe() + "'");
        const Type *t = promote(v->type());
        // ~x is x ^ -1, which needs no instruction of its own.
        ExprPtr ones(new Num(-1L));
        ones->setType(t);
        ExprPtr n(new Binary(BinOp::BitXor, convert(std::move(v), t), std::move(ones)));
        n->setType(t);
        return n;
    }
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
        ExprPtr n(new Num(static_cast<long>(measured->size(target_))));
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

        n = shiftOf(op, std::move(n), add());
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

// C's precedence, which famously puts the bitwise operators below comparison:
// "a & b == c" is "a & (b == c)". Honoured rather than corrected.
ExprPtr Parser::bitAnd() {
    ExprPtr n = equality();
    while (peek().is("&")) {
        std::size_t pos = peek().pos; at_++;
        n = arithmetic(BinOp::BitAnd, std::move(n), equality(), pos);
    }
    return n;
}

ExprPtr Parser::bitXor() {
    ExprPtr n = bitAnd();
    while (peek().is("^")) {
        std::size_t pos = peek().pos; at_++;
        n = arithmetic(BinOp::BitXor, std::move(n), bitAnd(), pos);
    }
    return n;
}

ExprPtr Parser::bitOr() {
    ExprPtr n = bitXor();
    while (peek().is("|")) {
        std::size_t pos = peek().pos; at_++;
        n = arithmetic(BinOp::BitOr, std::move(n), bitXor(), pos);
    }
    return n;
}

ExprPtr Parser::logicalAnd() {
    ExprPtr n = bitOr();
    while (peek().is("&&")) {
        std::size_t pos = peek().pos;
        at_++;
        ExprPtr r = decay(bitOr());
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
    if (dynamic_cast<const MemberAccess *>(&e)) return true;
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) return u->op() == '*';
    return false;
}

// Copies an lvalue so that a compound assignment can name the same place
// twice. Only the shapes whose evaluation costs nothing and changes nothing
// are copied: a name, a member of one, and a dereference of one.
//
// "*(p + i) += 1" is refused rather than duplicated. Evaluating p + i twice
// would be correct today, since nothing in an expression can have a side
// effect yet - but it would stop being correct the moment postfix ++ or a
// function call could appear there, and a wrong answer that appears later is
// worse than a refusal now.
ExprPtr Parser::cloneLvalue(const Expr &e, std::size_t pos) {
    if (const Var *v = dynamic_cast<const Var *>(&e)) {
        ExprPtr n(v->isLocal() ? Var::local(v->name(), v->offset())
                               : Var::global(v->name()));
        n->setType(v->type());
        return n;
    }
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(&e)) {
        ExprPtr obj = cloneLvalue(m->object(), pos);
        ExprPtr n(new MemberAccess(std::move(obj), m->name(), m->offset()));
        n->setType(m->type());
        return n;
    }
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '*') {
            if (const Var *pv = dynamic_cast<const Var *>(&u->operand())) {
                ExprPtr inner(pv->isLocal() ? Var::local(pv->name(), pv->offset())
                                            : Var::global(pv->name()));
                inner->setType(pv->type());
                ExprPtr n(new Unary('*', std::move(inner)));
                n->setType(u->type());
                return n;
            }
        }
    }
    src_.fail(pos, "this is too complicated to compound-assign to yet - "
                   "write it out as 'x = x op e'");
}

// The shift typing rule, which is not the usual arithmetic conversions: each
// operand promotes on its own and the result takes the left one's type.
ExprPtr Parser::shiftOf(BinOp op, ExprPtr lhs, ExprPtr rhs) {
    const Type *lt = promote(lhs->type());
    ExprPtr n(new Binary(op, convert(std::move(lhs), lt),
                             convert(std::move(rhs), promote(rhs->type()))));
    n->setType(lt);
    return n;
}

// x op= e becomes x = x op e. The target appears twice in the tree, which is
// correct while evaluating it has no side effect - the parser refuses a target
// that is anything but a name, a member or a dereference, and none of those
// can change anything by being evaluated.
ExprPtr Parser::compound(BinOp op, ExprPtr target, ExprPtr value, std::size_t pos) {
    if (!isLvalue(*target))
        src_.fail(pos, "left of a compound assignment is not something that can be assigned to");
    ExprPtr readBack = cloneLvalue(*target, pos);
    ExprPtr combined = (op == BinOp::Shl || op == BinOp::Shr)
        ? shiftOf(op, std::move(readBack), std::move(value))
        : arithmetic(op, std::move(readBack), std::move(value), pos);
    const Type *to = target->type();
    ExprPtr node(new Assign(std::move(target), convert(std::move(combined), to)));
    node->setType(to);
    return node;
}

ExprPtr Parser::incDec(ExprPtr target, bool increment, bool prefix, std::size_t pos) {
    if (!prefix)
        src_.fail(pos, "postfix '++' and '--' are not supported yet - "
                       "the old value needs a temporary this compiler has no way to make");
    ExprPtr one(new Num(1L));
    one->setType(types_.intType());
    return compound(increment ? BinOp::Add : BinOp::Sub, std::move(target),
                    std::move(one), pos);
}

// A null pointer constant, which C defines as an integer constant 0. It exists
// here so that "p ? q : 0" is accepted: that is the idiom, and refusing it
// would be refusing C rather than catching a type error.
static bool isNullConstant(const Expr &e) {
    const Num *n = dynamic_cast<const Num *>(&e);
    return n != nullptr && n->type()->isInteger() && n->value() == 0;
}

// cond ? a : b.
//
// The result type is the interesting part, and it is not "whatever the first
// arm was". C says the two arms are brought to one type by the usual arithmetic
// conversions when both are arithmetic, which is why "n ? 1 : 2.5" is a double
// even when the integer arm is the one taken. Both arms are converted here, so
// code generation sees one type and picks one register file.
ExprPtr Parser::conditional() {
    ExprPtr cond = logicalOr();
    if (!peek().is("?")) return cond;

    std::size_t pos = peek().pos;
    at_++;
    cond = decay(std::move(cond));
    requireScalar(*cond, pos, "the condition of '?:'");

    ExprPtr a = decay(expr());
    expect(":");
    // Right-associative through conditional() rather than expr(): the else arm
    // of "a ? b : c ? d : e" is the whole "c ? d : e".
    ExprPtr b = decay(conditional());

    const Type *ta = a->type();
    const Type *tb = b->type();
    const Type *result = nullptr;

    if (ta->isArithmetic() && tb->isArithmetic()) {
        result = usualArithmetic(ta, tb);
        a = convert(std::move(a), result);
        b = convert(std::move(b), result);
    } else if (ta == tb) {
        // Interned types, so this covers two pointers to the same thing and two
        // voids without asking what either is.
        result = ta;
    } else if (ta->isPointer() && isNullConstant(*b)) {
        result = ta;
        b = convert(std::move(b), result);
    } else if (tb->isPointer() && isNullConstant(*a)) {
        result = tb;
        a = convert(std::move(a), result);
    } else {
        src_.fail(pos, "the arms of '?:' have incompatible types '" +
                       ta->describe() + "' and '" + tb->describe() + "'");
    }

    if (result->isStructOrUnion())
        src_.fail(pos, "a struct or union in '?:' is not supported yet - "
                       "use a pointer to it");

    ExprPtr n(new Conditional(std::move(cond), std::move(a), std::move(b)));
    n->setType(result);
    return n;
}

ExprPtr Parser::assign() {
    ExprPtr n = conditional();

    static const struct { const char *tok; BinOp op; } kCompound[] = {
        { "+=", BinOp::Add }, { "-=", BinOp::Sub }, { "*=", BinOp::Mul },
        { "/=", BinOp::Div }, { "%=", BinOp::Mod }, { "&=", BinOp::BitAnd },
        { "|=", BinOp::BitOr }, { "^=", BinOp::BitXor },
        { "<<=", BinOp::Shl }, { ">>=", BinOp::Shr },
    };
    for (const auto &c : kCompound) {
        if (peek().is(c.tok)) {
            std::size_t pos = peek().pos; at_++;
            return compound(c.op, std::move(n), decay(assign()), pos);
        }
    }

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

    // "struct Point { int x; int y; };" declares a type and nothing else.
    if (peek().is(";")) { at_++; return StmtPtr(new Block({})); }

    if (sc == StorageTypedef) {
        Declared td = declarator(base);
        if (findTypedef(td.name)) src_.fail(td.pos, "'" + td.name + "' is typedefed twice");
        typedefIndex_[td.name] = typedefs_.size();
        typedefs_.push_back(TypedefName{ td.name, td.type });
        expect(";");
        return StmtPtr(new Block({}));
    }
    if (sc != StorageNone)
        src_.fail(peek().pos, "a storage class on a local is not supported yet");

    Declared d = declarator(base);
    if (!d.type->isComplete())
        src_.fail(d.pos, "'" + d.name + "' has an incomplete type");
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

// All three clauses are optional, so "for (;;)" is a loop with no condition.
// The initialiser may declare, and that declaration is scoped to the function
// rather than to the loop, because there is one flat scope per function - a
// simplification recorded in docs/TYPES.md and not introduced here.
StmtPtr Parser::forStatement() {
    expect("for");
    expect("(");
    // The initialiser's declaration belongs to the loop, not to what follows -
    // which is what lets two loops in one function both count with "i".
    enterScope();

    StmtPtr init;
    if (!consume(";")) {
        if (atDeclarationStart()) init = declaration();   // consumes its own ';'
        else { ExprPtr e = expr(); expect(";"); init = StmtPtr(new ExprStmt(std::move(e))); }
    }

    ExprPtr cond;
    if (!peek().is(";")) cond = decay(expr());
    expect(";");

    ExprPtr step;
    if (!peek().is(")")) step = decay(expr());
    expect(")");

    loopDepth_++;
    StmtPtr body = statement();
    loopDepth_--;

    leaveScope();
    return StmtPtr(new For(std::move(init), std::move(cond),
                           std::move(step), std::move(body)));
}

// An integer constant, where an expression cannot be parsed because there is
// nothing yet that could fold one. What is accepted is what case labels are
// actually written with: a number, a character constant, an enumerator, any of
// them signed. "case 1 + 2" is refused by name rather than mis-parsed, and it
// becomes possible the day a constant expression evaluator exists - which is
// also the day "int a[2 + 2]" and "enum { N = 1 << 4 }" start working, and is
// why it is one piece of work rather than three.
long Parser::constantValue(const char *what) {
    bool negate = false;
    if (consume("-")) negate = true;
    else consume("+");

    long v;
    if (peek().kind == TokenKind::Num) {
        if (peek().isFloat)
            src_.fail(peek().pos, std::string("expected ") + what +
                                  ", and a floating constant is not one");
        v = peek().value;
        at_++;
    } else if (peek().kind == TokenKind::Ident) {
        const EnumConst *e = findEnum(peek().text);
        if (!e)
            src_.fail(peek().pos, "'" + peek().text + "' is not a constant");
        v = e->value;
        at_++;
    } else {
        src_.fail(peek().pos, std::string("expected ") + what);
    }
    return negate ? -v : v;
}

long Parser::narrowTo(long v, const Type *t) const {
    int bits = t->size(target_) * 8;
    if (bits >= 64) return v;
    unsigned long mask = (1UL << bits) - 1;
    unsigned long kept = static_cast<unsigned long>(v) & mask;
    // Sign-extend back out, so the value held here is the same 64-bit pattern
    // %rax will hold for that type - which is what lets code generation compare
    // the whole register and ask nothing about width.
    if (t->isSigned(target_) && (kept & (1UL << (bits - 1)))) kept |= ~mask;
    return static_cast<long>(kept);
}

// switch (cond) body.
//
// The controlling expression is promoted and every case value is converted to
// that promoted type, which is what C says and what makes the comparison a
// single form: both sides are already the same type before code generation sees
// either of them.
StmtPtr Parser::switchStatement() {
    std::size_t pos = peek().pos;
    expect("switch");
    expect("(");
    ExprPtr cond = decay(expr());
    if (!cond->type()->isInteger())
        src_.fail(pos, "a switch needs an integer, not '" +
                       cond->type()->describe() + "'");
    const Type *governing = promote(cond->type());
    cond = convert(std::move(cond), governing);
    expect(")");

    switches_.push_back(SwitchCtx{ {}, nullptr, governing });
    switchDepth_++;
    StmtPtr body = statement();
    switchDepth_--;

    SwitchCtx ctx = std::move(switches_.back());
    switches_.pop_back();
    return StmtPtr(new Switch(std::move(cond), std::move(body),
                              std::move(ctx.cases), ctx.deflt));
}

// "case v:" and "default:", each labelling one statement.
StmtPtr Parser::caseLabel() {
    std::size_t pos = peek().pos;
    bool isDefault = consume("default");
    if (!isDefault) expect("case");

    if (switches_.empty())
        src_.fail(pos, isDefault ? "'default' is not inside a switch"
                                 : "'case' is not inside a switch");

    long value = 0;
    if (isDefault) {
        if (switches_.back().deflt)
            src_.fail(pos, "a switch has only one 'default'");
    } else {
        value = narrowTo(constantValue("a case value"),
                         switches_.back().governing);
        for (const Case *c : switches_.back().cases)
            if (c->value() == value)
                src_.fail(pos, "duplicate case value " + std::to_string(value));
        // "case 1 + 2:" parses the 1, then finds a '+' where the colon should
        // be. Reporting the colon describes what the parser wanted rather than
        // what the rule is, and the rule is the thing that can be worked around.
        if (!peek().is(":"))
            src_.fail(peek().pos, "a case value must be a single integer "
                                  "constant - there is no constant expression "
                                  "evaluator yet");
    }
    expect(":");

    // A label labels a statement, and a declaration is not one. Saying so is
    // worth the line: the alternative is the expression parser reporting the
    // type name as a stray token, which describes neither the rule nor the fix.
    if (atDeclarationStart())
        src_.fail(peek().pos, "a label cannot be followed by a declaration - "
                              "put it in a block");
    if (peek().is("}"))
        src_.fail(peek().pos, "a label must be followed by a statement");

    StmtPtr body = statement();

    Case *node = new Case(value, isDefault, caseIds_++, std::move(body));
    StmtPtr owned(node);
    // Re-read the top of the stack rather than holding a reference across the
    // body above: a nested switch pushes onto this vector, and a push that
    // reallocates leaves any reference taken before it dangling.
    SwitchCtx &sw = switches_.back();
    if (isDefault) sw.deflt = node;
    else sw.cases.push_back(node);
    return owned;
}

// name: statement.
//
// Reached only when the caller has already seen an identifier followed by a
// colon, which is the one lookahead the statement grammar needs: an identifier
// at the start of a statement is otherwise an expression, and "x" alone cannot
// say which.
StmtPtr Parser::gotoLabel() {
    std::size_t pos = peek().pos;
    std::string name = expectIdent("a label");
    expect(":");

    for (const LabelDef &l : labels_)
        if (l.name == name)
            src_.fail(pos, "label '" + name + "' is defined twice in this function");
    labels_.push_back(LabelDef{ name, pos });

    if (atDeclarationStart())
        src_.fail(peek().pos, "a label cannot be followed by a declaration - "
                              "put it in a block");
    if (peek().is("}"))
        src_.fail(peek().pos, "a label must be followed by a statement");

    return StmtPtr(new Label(std::move(name), statement()));
}

// A goto names a label somewhere in the same function, and "somewhere" includes
// further down. So this runs when the body is closed and not before.
void Parser::resolveGotos() {
    for (const LabelDef &g : gotos_) {
        bool found = false;
        for (const LabelDef &l : labels_)
            if (l.name == g.name) { found = true; break; }
        if (!found)
            src_.fail(g.pos, "no label '" + g.name + "' in this function");
    }
    labels_.clear();
    gotos_.clear();
}

StmtPtr Parser::block() {
    expect("{");
    enterScope();
    std::vector<StmtPtr> body;
    while (!peek().is("}")) {
        if (peek().kind == TokenKind::End)
            src_.fail(peek().pos, "unclosed '{'");
        body.push_back(atDeclarationStart() ? declaration() : statement());
    }
    expect("}");
    leaveScope();
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
        loopDepth_++;
        StmtPtr body = statement();
        loopDepth_--;
        return StmtPtr(new While(std::move(cond), std::move(body)));
    }

    if (peek().is("for")) return forStatement();

    if (consume("do")) {
        loopDepth_++;
        StmtPtr body = statement();
        loopDepth_--;
        expect("while");
        expect("(");
        ExprPtr cond = decay(expr());
        expect(")");
        expect(";");
        return StmtPtr(new DoWhile(std::move(body), std::move(cond)));
    }

    if (peek().is("switch")) return switchStatement();
    if (peek().is("case") || peek().is("default")) return caseLabel();

    if (consume("goto")) {
        std::size_t pos = peek().pos;
        std::string name = expectIdent("a label to jump to");
        expect(";");
        gotos_.push_back(LabelDef{ name, pos });
        return StmtPtr(new Goto(std::move(name)));
    }

    // An identifier followed by a colon is a label; an identifier followed by
    // anything else starts an expression. One token of lookahead separates
    // "done: return 0;" from "done = 0;", and nothing else in the statement
    // grammar needs any.
    if (peek().kind == TokenKind::Ident && peekAt(1).is(":")) return gotoLabel();

    // break leaves a loop or a switch, whichever is nearer. continue takes only
    // a loop, and looks past a switch to find it.
    if (consume("break")) {
        if (loopDepth_ == 0 && switchDepth_ == 0)
            src_.fail(peek().pos, "'break' is not inside a loop or a switch");
        expect(";");
        return StmtPtr(new Break());
    }

    if (consume("continue")) {
        if (loopDepth_ == 0)
            src_.fail(peek().pos, "'continue' is not inside a loop");
        expect(";");
        return StmtPtr(new Continue());
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

    // A type declaration on its own, with no object: "struct Point { ... };"
    if (peek().is(";")) { at_++; return; }

    if (sc == StorageTypedef) {
        Declared td = declarator(base);
        if (findTypedef(td.name)) src_.fail(td.pos, "'" + td.name + "' is typedefed twice");
        typedefIndex_[td.name] = typedefs_.size();
        typedefs_.push_back(TypedefName{ td.name, td.type });
        expect(";");
        return;
    }

    // The declarator has to be read before it is known whether this declares a
    // function or an object: "int *f(void)" and "int *p" begin identically.
    locals_.clear();
    scopeStarts_.clear();
    enterScope();                 // the parameters live here
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
        globalIndex_[d.name] = globals_.size();
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
    bool variadic = false;

    if (!consume(")")) {
        if (peek().is("void") && peekAt(1).is(")")) {
            at_ += 2;
        } else {
            for (;;) {
                if (consume("...")) { variadic = true; expect(")"); break; }
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
    for (const Type *pt : params)
        if (pt->isStructOrUnion())
            src_.fail(d.pos, "passing a struct or union by value is not supported yet - "
                             "pass a pointer to it");
    if (d.type->isStructOrUnion())
        src_.fail(d.pos, "returning a struct or union by value is not supported yet");
    if (static_cast<int>(params.size()) > kMaxArgs)
        src_.fail(d.pos, "more than " + std::to_string(kMaxArgs) +
                         " parameters is not supported yet");

    if (consume(";")) {
        declareFunction(d.name, d.type, params, variadic, false, d.pos);
        return;
    }
    if (variadic)
        src_.fail(d.pos, "defining a variadic function is not supported yet");

    declareFunction(d.name, d.type, params, variadic, true, d.pos);
    returnType_ = d.type;

    StmtPtr body = block();
    resolveGotos();

    int frame = alignTo(frameSize_, 16);
    program.functions.push_back(Function(d.name, d.type, std::move(paramSlots),
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
