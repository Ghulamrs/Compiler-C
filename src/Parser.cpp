#include "Parser.h"
#include "Source.h"

#include <climits>
#include <cstring>

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
                                     "struct", "union", "enum",
                                     // Qualifiers start a type too: "(const
                                     // int)x" is a cast, and specifiers()
                                     // consumes them inside the same loop that
                                     // tests this - so adding them here without
                                     // consuming them there would spin.
                                     "const", "volatile" };
    for (const char *k : t)
        if (peek().is(k)) return true;
    return peek().kind == TokenKind::Ident && findTypedef(peek().text) != nullptr;
}

bool Parser::atDeclarationStart() const {
    return atTypeName() || peek().is("static") || peek().is("extern")
        || peek().is("register");
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
    int widest = 1;
    // Layout runs in bits, not bytes, because bit-fields make the cursor land
    // between them. An ordinary member rounds it up to a byte first, so nothing
    // outside this loop has to know the difference.
    long bitCursor = 0;
    long widestBits = 0;      // a union's size, measured the same way

    while (!peek().is("}")) {
        if (peek().kind == TokenKind::End) src_.fail(pos, "unclosed '{'");
        StorageClass msc;
        const Type *base = specifiers(&msc);
        if (msc != StorageNone)
            src_.fail(peek().pos, "a storage class on a member is not supported yet");
        for (;;) {
            // An unnamed bit-field: "int : 3;" pads, and "int : 0;" moves to the
            // next storage unit. Neither has a declarator at all, which is why
            // this is tested before one is read.
            if (peek().is(":")) {
                std::size_t cpos = peek().pos;
                at_++;
                long w = constantExpression("a bit-field width");
                if (!base->isInteger())
                    src_.fail(cpos, "a bit-field must have an integer type, not '" +
                                    base->describe() + "'");
                long unitBits = base->size(target_) * 8;
                if (w < 0 || w > unitBits)
                    src_.fail(cpos, "a bit-field of " + std::to_string(w) +
                                    " bits does not fit in '" + base->describe() + "'");
                int a = base->align(target_);
                if (a > widest) widest = a;
                if (w == 0) {
                    // Width zero says: start the next one at a fresh unit. It is
                    // the only way to force padding without naming a member.
                    bitCursor = alignTo(bitCursor, unitBits);
                } else if (kind != Kind::Union) {
                    if (bitCursor % unitBits + w > unitBits)
                        bitCursor = alignTo(bitCursor, unitBits);
                    bitCursor += w;
                }
                if (kind == Kind::Union && w > unitBits) w = unitBits;
                if (kind == Kind::Union && w > widestBits) widestBits = w;
                if (!consume(",")) break;
                continue;
            }

            Declared d = declarator(base);

            if (peek().is(":")) {
                std::size_t cpos = peek().pos;
                at_++;
                long w = constantExpression("a bit-field width");
                if (!d.type->isInteger())
                    src_.fail(d.pos, "a bit-field must have an integer type, not '" +
                                     d.type->describe() + "'");
                long unitBits = d.type->size(target_) * 8;
                if (w < 0)
                    src_.fail(cpos, "'" + d.name + "' has a bit-field width of " +
                                    std::to_string(w) + ", which cannot be negative");
                if (w == 0)
                    src_.fail(cpos, "'" + d.name + "' has a bit-field width of 0; "
                                    "only an unnamed bit-field may be zero, and it "
                                    "means 'start the next storage unit'");
                if (w > unitBits)
                    src_.fail(cpos, "'" + d.name + "' is " + std::to_string(w) +
                                    " bits, which does not fit in '" +
                                    d.type->describe() + "'");

                int a = d.type->align(target_);
                if (a > widest) widest = a;

                long at, bitOff;
                if (kind == Kind::Union) {
                    at = 0;
                    bitOff = 0;
                    if (w > widestBits) widestBits = w;
                } else {
                    // The ABI rule: a bit-field never straddles a boundary of
                    // its own declared type. If it would, it starts at the next
                    // one, and the gap is padding.
                    if (bitCursor % unitBits + w > unitBits)
                        bitCursor = alignTo(bitCursor, unitBits);
                    at = (bitCursor / unitBits) * d.type->size(target_);
                    bitOff = bitCursor % unitBits;
                    bitCursor += w;
                }
                members.push_back(Member{ d.name, d.type, static_cast<int>(at),
                                          static_cast<int>(w),
                                          static_cast<int>(bitOff) });
                if (!consume(",")) break;
                continue;
            }

            if (!d.type->isComplete())
                src_.fail(d.pos, "'" + d.name + "' has an incomplete type");
            int a = d.type->align(target_);
            if (a > widest) widest = a;
            // An ordinary member starts on a byte, and then on a multiple of its
            // own alignment. A union stacks every member at zero.
            long byteCursor = (bitCursor + 7) / 8;
            long at = (kind == Kind::Union) ? 0 : alignTo(byteCursor, a);
            members.push_back(Member{ d.name, d.type, static_cast<int>(at) });
            long endBits = (at + d.type->size(target_)) * 8;
            if (kind == Kind::Union) { if (endBits > widestBits) widestBits = endBits; }
            else bitCursor = endBits;
            if (!consume(",")) break;
        }
        expect(";");
    }
    expect("}");

    // An unnamed bit-field declares no member, so a body of nothing but padding
    // is still a body. It is the size that has to be non-zero, not the list.
    long totalBits = (kind == Kind::Union) ? widestBits : bitCursor;
    if (members.empty() && totalBits == 0)
        src_.fail(pos, std::string(what) + " has no members");

    int size = static_cast<int>(alignTo((totalBits + 7) / 8, widest));
    type->complete(members, size, widest);
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
        // An enumerator is an int, so the constant is narrowed to one - which
        // is also what makes "enum { Big = 1 << 40 }" wrong rather than large.
        if (consume("="))
            next = narrowTo(constantExpression("a constant"), types_.intType());
        enumIndex_[name] = enums_.size();
        enums_.push_back(EnumConst{ name, next });
        next = next + 1;
        if (!consume(",")) break;
    }
    expect("}");
    if (enums_.empty()) src_.fail(pos, "enum has no enumerators");
    return types_.intType();
}

const Type *Parser::specifiers(StorageClass *storage, Qualifiers *quals) {
    std::size_t start = peek().pos;
    *storage = StorageNone;
    Qualifiers discard;
    if (quals == nullptr) quals = &discard;

    for (;;) {
        if (consume("static"))  { *storage = StorageStatic; continue; }
        if (consume("extern"))  { *storage = StorageExtern; continue; }
        if (consume("typedef")) { *storage = StorageTypedef; continue; }
        if (consume("const"))    { quals->isConst = true; continue; }
        if (consume("volatile")) { quals->isVolatile = true; continue; }
        if (peek().is("register"))
            src_.fail(peek().pos, "'register' is not supported yet - it is a hint "
                                  "this compiler has no way to take, since every "
                                  "value already goes through memory");
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
        // A qualifier may sit either side of the type: "const int" and
        // "int const" are the same declaration.
        if (consume("const"))         { quals->isConst = true; continue; }
        if (consume("volatile"))      { quals->isVolatile = true; continue; }
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
// A declarator, read the way C defines one: recursively.
//
// The knot it unties is that the suffix binds tighter than the prefix. In
// "int *p[10]" the [10] applies first, so p is an array of pointers; the
// parentheses in "int (*p)[10]" undo that and make it a pointer to an array.
// No amount of left-to-right scanning gets there, because what the thing inside
// the parentheses is a declarator *of* is not known until what follows the ')'
// has been read.
//
// So the parenthesised part is read twice. Once against a placeholder type,
// purely to find its matching ')' - the result is thrown away. Then the suffix
// after the ')' is applied to the base, giving the type the inner declarator
// really modifies, and the parser rewinds and reads it again for real. Reading
// it twice is the cost of not building a declarator tree to walk afterwards,
// and at the size of a declarator that is a cost worth paying.
const Type *Parser::arraySuffix(const Type *base, std::size_t pos) {
    std::vector<long> dims;
    while (consume("[")) {
        // "char s[]" - an array with no length. Legal as a parameter, where it
        // means a pointer anyway, and as "extern int a[]" where another unit
        // gives it a size. Anywhere else it produces an incomplete type, and
        // the declaration that tried to use it says so.
        if (consume("]")) { dims.push_back(-1); continue; }
        std::size_t dpos = peek().pos;
        long n = constantExpression("an array length");
        // Caught here rather than left to produce a type nothing can hold: a
        // negative length reaches the layout code as a size, and a size is
        // where it would stop being obvious what went wrong.
        if (n <= 0)
            src_.fail(dpos, "an array length must be positive, not " +
                            std::to_string(n));
        dims.push_back(n);
        expect("]");
    }
    // Only the outermost dimension may be left out: "int a[][3]" is an unknown
    // number of rows of three, and "int a[3][]" is rows of unknown width, which
    // has no size to step by.
    for (std::size_t i = 1; i < dims.size(); i++)
        if (dims[i] < 0)
            src_.fail(pos, "only the first dimension may be left empty - the "
                           "others decide how far one step moves");

    // Applied in reverse, so a[2][3] is two of three rather than three of two.
    for (std::size_t i = dims.size(); i-- > 0; )
        base = types_.arrayOf(base, dims[i]);
    return base;
}

Parser::Declared Parser::declarator(const Type *base, bool nameOptional) {
    while (consume("*")) base = types_.pointerTo(base);

    if (peek().is("(")) {
        std::size_t open = at_;
        at_++;
        // Whether the parentheses are undoing anything. "(*p)" is a pointer
        // declarator and "(f)" is just a name wearing brackets - which matters
        // below, where only the first of them can become a function pointer.
        bool wrapsAPointer = peek().is("*");

        // First reading: against a placeholder, to find the ')'. Whatever type
        // this produces is meaningless and is discarded.
        declarator(types_.intType(), true);
        expect(")");

        // "int (*f)(void)" - a pointer to a function. The declarator inside the
        // parentheses modifies not int but "function returning int", so that
        // type is built here, before the second reading, and the '*' inside is
        // what then makes it a pointer.
        //
        // "int (f)(void)" is not that. The parentheses there undo nothing and
        // the '(' after them opens an ordinary parameter list, which the caller
        // reads for itself - which is why this asks whether the parentheses
        // wrapped a '*'.
        std::size_t posOuter = peek().pos;
        const Type *outer;
        if (peek().is("(") && wrapsAPointer) {
            std::vector<const Type *> params;
            bool variadic = false;
            parameterTypes(params, variadic);
            outer = types_.functionType(base, std::move(params), variadic);
        } else {
            outer = arraySuffix(base, posOuter);
        }
        std::size_t after = at_;

        // Second reading: the same tokens, now against the type they actually
        // modify.
        at_ = open + 1;
        Declared inner = declarator(outer, nameOptional);
        expect(")");
        at_ = after;
        return inner;
    }

    std::size_t pos = peek().pos;
    // A prototype may name only types. The name is left empty rather than
    // invented, so that everything downstream can tell the difference between a
    // parameter called nothing and one called something.
    std::string name;
    if (nameOptional && peek().kind != TokenKind::Ident) name.clear();
    else name = expectIdent("a name");

    return Declared{ name, arraySuffix(base, pos), pos };
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
    // The element type is read out before the move. As two arguments to one
    // call the order between them is unspecified, and the order that moves
    // first leaves the other reading an emptied pointer.
    const Type *to = types_.pointerTo(e->type()->pointee());
    return ExprPtr(new Cast(to, std::move(e)));
}

void Parser::requireScalar(const Expr &e, std::size_t pos, const char *what) {
    if (!e.type()->isScalar())
        src_.fail(pos, std::string(what) + " needs a number or a pointer, not '" +
                       e.type()->describe() + "'");
}

// A null pointer constant, which C defines as an integer constant 0. Two things
// need it and they are the same rule: "p ? q : 0" must be accepted, because
// that is the idiom and refusing it would be refusing C; and 0 must reach a
// pointer parameter, because that is what NULL expands to and what every call
// like fgets(line, 64, 0) relies on.
static bool isNullConstant(const Expr &e) {
    const Num *n = dynamic_cast<const Num *>(&e);
    return n != nullptr && n->type()->isInteger() && n->value() == 0;
}

// The constraints on simple assignment, which an argument has to satisfy
// against its parameter. Everything permitted is listed and anything else is
// refused, which is the safe direction to be wrong in: a case missing here is a
// program rejected with a message, never a program quietly miscompiled.
void Parser::checkAssignable(const Expr &from, const Type *to, std::size_t pos,
                             const std::string &what) const {
    const Type *ft = from.type();

    // Types are interned, so one comparison settles every case where the two
    // are the same type reached by different routes.
    if (ft == to) return;

    // Every arithmetic conversion is defined, and the Cast inserted after this
    // check is what performs it.
    if (ft->isArithmetic() && to->isArithmetic()) return;

    auto refuse = [&](const char *tail) {
        src_.fail(pos, what + " is '" + to->describe() + "' and this is '" +
                       ft->describe() + "'" + tail);
    };

    if (to->isPointer() && ft->isPointer()) {
        // void * in either direction, which is the whole reason malloc and free
        // need no special knowledge here.
        if (to->pointee()->isVoid() || ft->pointee()->isVoid()) return;
        refuse(" - a cast says you meant it");
    }
    if (to->isPointer() && ft->isInteger()) {
        if (isNullConstant(from)) return;
        refuse(" - only the constant 0 becomes a pointer on its own");
    }
    if (to->isArithmetic() && ft->isPointer())
        refuse(" - a pointer is not a number here, though a cast makes it one");

    // Anything left is a struct, a union or a void against something it is not.
    // The interned comparison above has already said they differ.
    refuse("");
}

// ---- symbols ----

void Parser::enterScope() { scopeStarts_.push_back(locals_.size()); }

void Parser::leaveScope() {
    locals_.resize(scopeStarts_.back());
    scopeStarts_.pop_back();
}

int Parser::allocateFrameSlot(const Type *type) {
    frameSize_ += type->size(target_);
    frameSize_ = alignTo(frameSize_, type->align(target_));
    return frameSize_;
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

    int offset = allocateFrameSlot(type);
    locals_.push_back(Local{ name, offset, type, false, std::string() });
    return offset;
}

// A static local takes no frame slot: its storage is in the data section and
// outlives the call, which is the whole of what the keyword means here. The
// duplicate check is the same one ordinary locals get, because the scope rule
// is the same - only the storage differs.
void Parser::declareStaticLocal(const std::string &name, const Type *type,
                                std::size_t pos, const std::string &symbol) {
    if (type->isVoid())
        src_.fail(pos, "'" + name + "' cannot have type void");
    std::size_t from = scopeStarts_.empty() ? 0 : scopeStarts_.back();
    for (std::size_t i = from; i < locals_.size(); i++)
        if (locals_[i].name == name)
            src_.fail(pos, "'" + name + "' is declared twice in this block");
    locals_.push_back(Local{ name, 0, type, false, symbol });
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

Parser::GlobalSym *Parser::findGlobalToUpdate(const std::string &name) {
    auto it = globalIndex_.find(name);
    return it == globalIndex_.end() ? nullptr : &globals_[it->second];
}

ExprPtr Parser::defaultPromote(ExprPtr e) {
    if (e->type()->kind() == Kind::Float)
        return convert(std::move(e), types_.doubleType());
    if (e->type()->isInteger()) {
        // Read the promoted type before moving. Written as one expression the
        // two are unsequenced arguments, and clang evaluates the move first -
        // which emptied e and crashed the compiler on every variadic argument.
        const Type *to = promote(e->type());
        return convert(std::move(e), to);
    }
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

const Parser::Signature *Parser::findFunction(const std::string &name) const {
    auto it = functionIndex_.find(name);
    return it != functionIndex_.end() ? &functions_[it->second] : nullptr;
}

const Parser::Signature &Parser::lookupFunction(const std::string &name,
                                                std::size_t pos) const {
    if (const Signature *s = findFunction(name)) return *s;
    src_.fail(pos, "'" + name + "' was not declared - a prototype must come first");
}

// The parameter list of a function type. Names are permitted and thrown away:
// "int (*f)(int n)" declares nothing called n, and entering one would put a
// name in the symbol table that no body can see. That is the whole difference
// from the list a definition parses.
void Parser::parameterTypes(std::vector<const Type *> &params, bool &variadic) {
    expect("(");
    variadic = false;
    if (consume(")")) return;
    if (peek().is("void") && peekAt(1).is(")")) { at_ += 2; return; }

    for (;;) {
        if (consume("...")) { variadic = true; expect(")"); break; }
        StorageClass psc;
        Qualifiers pquals;
        const Type *pt = specifiers(&psc, &pquals);
        Declared pd = declarator(pt, true);
        // A parameter declared as an array is a pointer, the same rule and the
        // same reason as in a definition.
        if (pd.type->isArray()) pd.type = types_.pointerTo(pd.type->pointee());
        if (pd.type->isVoid())
            src_.fail(pd.pos, "'void' is only a parameter list on its own");
        params.push_back(pd.type);
        if (consume(")")) break;
        expect(",");
    }
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
        // The lexer keeps a literal's bit pattern in a long because that is
        // what Num holds, but a literal is never negative in C - so the width
        // is decided by reading it back as unsigned. Compared as a long,
        // 18446744073709551615 looks like -1 and would be given type int.
        unsigned long u = static_cast<unsigned long>(t.value);
        if (t.suffixU && t.suffixL)      ty = types_.get(Kind::ULong);
        else if (t.suffixU)              ty = u <= UINT_MAX
                                            ? types_.get(Kind::UInt) : types_.get(Kind::ULong);
        else if (t.suffixL)              ty = u <= LONG_MAX
                                            ? types_.get(Kind::Long) : types_.get(Kind::ULong);
        else if (u <= INT_MAX)           ty = types_.intType();
        else if (u <= LONG_MAX)          ty = types_.get(Kind::Long);
        // Too large for a signed long. C89 gives an unsuffixed decimal
        // constant the first of int, long, unsigned long that holds it.
        else                             ty = types_.get(Kind::ULong);
        ExprPtr n(new Num(t.value));
        n->setType(ty);
        at_++;
        return n;
    }

    if (peek().kind == TokenKind::Ident) {
        std::string name = peek().text;
        std::size_t pos = peek().pos;

        // A name before a '(' is usually a call by name. It is not when the
        // name denotes an object holding a pointer to a function: then the '('
        // belongs to postfix(), which calls through any expression of that
        // type and so handles "f(1)", "table[i](1)" and "s.op(1)" by one route
        // instead of three. The symbol table is the only thing that can tell
        // the two apart, so it is asked before the token is claimed.
        const Local *l = findLocal(name);
        const GlobalSym *g = l != nullptr ? nullptr : findGlobal(name);
        const Type *held = l != nullptr ? l->type : (g != nullptr ? g->type : nullptr);
        bool callsThroughObject = held != nullptr && held->isFunctionPointer();

        if (peekAt(1).is("(") && !callsThroughObject) {
            at_ += 2;                       // the name and the '('
            const Signature &sig = lookupFunction(name, pos);
            return finishCall(name, nullptr, sig.returns, sig.params,
                              sig.variadic, pos);
        }

        at_++;
        if (const EnumConst *e = findEnum(name)) {
            ExprPtr n(new Num(e->value));
            n->setType(types_.intType());
            return n;
        }
        if (ExprPtr v = objectRef(name)) return v;

        // A function's name, used as a value rather than called, is a pointer
        // to it. C says the conversion happens on its own - "qsort(a, n, s, cmp)"
        // has no '&' in it and never has - so it is done here rather than being
        // demanded of the program.
        //
        // The node is the address of the function's symbol, which is exactly
        // what '&' on a global already generates. Nothing new reaches code
        // generation.
        if (const Signature *sig = findFunction(name)) {
            Var *v = Var::global(name);
            ExprPtr target(v);
            const Type *fn = types_.functionType(sig->returns, sig->params,
                                                 sig->variadic);
            target->setType(fn);
            ExprPtr n(new Unary('&', std::move(target)));
            n->setType(types_.pointerTo(fn));
            return n;
        }
        src_.fail(pos, "'" + name + "' was not declared");
    }

    src_.fail(peek().pos, "expected an expression");
}

// ---- initialisers ----

Parser::Init Parser::parseInitialiser() {
    Init in;
    in.pos = peek().pos;
    if (consume("{")) {
        in.isList = true;
        if (peek().is("}"))
            src_.fail(in.pos, "an initialiser list needs at least one value");
        for (;;) {
            in.items.push_back(parseInitialiser());
            if (consume("}")) break;
            expect(",");
            // A trailing comma before the brace, which C allows and which
            // matters to anyone generating C or editing a list by lines.
            if (consume("}")) break;
        }
        return in;
    }
    // Not decayed. A string literal initialising a char array has to be seen as
    // the array it is - decay would make it a char * and "char s[8] = \"abc\""
    // would look like an attempt to put a pointer in an array. The decay
    // happens instead where a scalar is stored, which is the only place it is
    // wanted.
    in.value = assign();
    return in;
}

// A string literal is the one initialiser that is neither a list nor a scalar:
// "char s[4] = \"abc\"" copies the characters into the array rather than
// pointing at them. Only for an array of a character type - "int a[4]" is not
// something C lets a string initialise.
const StrLit *Parser::stringInitialiser(const Init &in, const Type *type) {
    if (in.isList || !type->isArray()) return nullptr;
    Kind e = type->pointee()->kind();
    if (e != Kind::Char && e != Kind::SChar && e != Kind::UChar) return nullptr;
    return dynamic_cast<const StrLit *>(in.value.get());
}

long Parser::inferredLength(const Init &in, const Type *element, std::size_t pos) {
    if (const StrLit *s = stringInitialiser(in, types_.arrayOf(element, 1)))
        return static_cast<long>(s->text().size()) + 1;   // the zero counts
    if (!in.isList)
        src_.fail(pos, "an array with no length needs a braced initialiser to "
                       "count, or a string to measure");
    return static_cast<long>(in.items.size());
}

// The lvalue of the piece the path leads to, built from the name each time
// rather than cloned. objectRef consults the symbol table, so a local, a global
// and a static local are all reached the same way.
ExprPtr Parser::targetFor(const std::string &name,
                          const std::vector<InitStep> &path) {
    ExprPtr e = objectRef(name);
    for (const InitStep &s : path) {
        if (s.member != nullptr) {
            const Member *m = s.member;
            ExprPtr acc(new MemberAccess(std::move(e), m->name, m->offset,
                                         m->width, m->bitOffset));
            acc->setType(m->type);
            e = std::move(acc);
        } else {
            const Type *elem = e->type()->pointee();
            ExprPtr index(new Num(s.index));
            index->setType(types_.intType());
            ExprPtr sum = pointerAdd(decay(std::move(e)), std::move(index));
            ExprPtr deref(new Unary('*', std::move(sum)));
            deref->setType(elem);
            e = std::move(deref);
        }
    }
    return e;
}

// One scalar at a time, and whatever the initialiser did not mention is zeroed
// - which is what C says a partly-initialised aggregate holds, and the reason
// "struct P p = {0};" is the idiom it is.
//
// The cost is worth stating rather than hiding: every element is a store, so
// "char buf[1024] = {0}" is a thousand of them where a memset would do. Correct
// and slow, in a way a later pass could fix by noticing a run of zeroes.
void Parser::emitInit(const std::string &name, std::vector<InitStep> &path,
                      const Type *type, Init &in, std::vector<StmtPtr> &out) {
    auto store = [&](ExprPtr target, ExprPtr value, std::size_t pos) {
        const Type *to = target->type();
        checkAssignable(*value, to, pos, "'" + name + "'");
        ExprPtr a(new Assign(std::move(target), convert(std::move(value), to)));
        a->setType(to);
        out.push_back(StmtPtr(new ExprStmt(std::move(a))));
    };
    auto zeroScalar = [&](std::size_t pos) {
        ExprPtr target = targetFor(name, path);
        const Type *t = target->type();
        ExprPtr z;
        if (t->isFloating()) { z.reset(new Num(0.0)); z->setType(types_.doubleType()); }
        else                 { z.reset(new Num(0L));  z->setType(types_.intType()); }
        store(std::move(target), std::move(z), pos);
    };
    // An element or member the list did not reach. An aggregate recurses with
    // an empty list so that its own pieces are zeroed in turn.
    auto fillRest = [&](const Type *t, std::size_t pos) {
        if (t->isArray() || t->isStructOrUnion()) {
            Init empty;
            empty.isList = true;
            empty.pos = pos;
            emitInit(name, path, t, empty, out);
        } else {
            zeroScalar(pos);
        }
    };

    if (type->isArray()) {
        long len = type->length();
        const Type *elem = type->pointee();

        if (const StrLit *s = stringInitialiser(in, type)) {
            const std::string &text = s->text();
            if (static_cast<long>(text.size()) > len)
                src_.fail(in.pos, "'" + name + "' holds " + std::to_string(len) +
                                  " characters and the string has " +
                                  std::to_string(text.size()));
            for (long i = 0; i < len; i++) {
                path.push_back(InitStep{ nullptr, i });
                long ch = i < static_cast<long>(text.size())
                        ? static_cast<long>(static_cast<unsigned char>(
                              text[static_cast<std::size_t>(i)]))
                        : 0L;
                ExprPtr c(new Num(ch));
                c->setType(types_.intType());
                store(targetFor(name, path), std::move(c), in.pos);
                path.pop_back();
            }
            return;
        }

        if (!in.isList)
            src_.fail(in.pos, "'" + name + "' is an array and needs a braced initialiser");
        if (static_cast<long>(in.items.size()) > len)
            src_.fail(in.pos, "'" + name + "' has " + std::to_string(len) +
                              " elements and its initialiser has " +
                              std::to_string(in.items.size()));

        for (long i = 0; i < len; i++) {
            path.push_back(InitStep{ nullptr, i });
            if (i < static_cast<long>(in.items.size()))
                emitInit(name, path, elem, in.items[static_cast<std::size_t>(i)], out);
            else
                fillRest(elem, in.pos);
            path.pop_back();
        }
        return;
    }

    if (type->isStructOrUnion()) {
        // "struct P p = q;" and "struct P p = f();" are not aggregate
        // initialisers at all - they are the copy that '=' does between two
        // objects of one type, and checkAssignable is what says they are.
        if (!in.isList) {
            store(targetFor(name, path), decay(std::move(in.value)), in.pos);
            return;
        }
        const std::vector<Member> &members = type->members();
        // A union takes one initialiser and it belongs to the first member,
        // which is all C89 offers without designators.
        std::size_t count = type->kind() == Kind::Union
                          ? (members.empty() ? std::size_t(0) : std::size_t(1))
                          : members.size();
        if (in.items.size() > count)
            src_.fail(in.pos, "'" + name + "' takes " + std::to_string(count) +
                              " initialiser(s) and was given " +
                              std::to_string(in.items.size()));

        for (std::size_t i = 0; i < count; i++) {
            const Member &m = members[i];
            // An unnamed bit-field is padding that happens to have a width. It
            // has no lvalue, so there is nothing to store into.
            if (m.name.empty()) continue;
            path.push_back(InitStep{ &m, 0 });
            if (i < in.items.size()) emitInit(name, path, m.type, in.items[i], out);
            else                     fillRest(m.type, in.pos);
            path.pop_back();
        }
        return;
    }

    // A scalar. C allows braces round one - "int x = { 5 };" - and means the
    // same by it.
    if (in.isList) {
        if (in.items.size() != 1)
            src_.fail(in.pos, "'" + name + "' is not an aggregate and takes one value");
        emitInit(name, path, type, in.items[0], out);
        return;
    }
    store(targetFor(name, path), decay(std::move(in.value)), in.pos);
}

// A constant that may be floating. The integer folder cannot answer this: it
// works in longs, and 1.5 is not one. Small on purpose - a file-scope
// initialiser is a literal, a sign, and the conversions the parser inserted
// around them.
static bool foldDouble(const Expr &e, double *out) {
    if (const Num *n = dynamic_cast<const Num *>(&e)) {
        *out = n->type()->isFloating() ? n->dvalue()
                                       : static_cast<double>(n->value());
        return true;
    }
    if (const Cast *c = dynamic_cast<const Cast *>(&e)) return foldDouble(c->value(), out);
    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        if (u->op() == '-' && foldDouble(u->operand(), out)) { *out = -*out; return true; }
    }
    return false;
}

// The same initialiser as data rather than as statements. A file-scope object
// is laid out before the program runs, so every scalar in it has to fold to a
// constant - and the pieces come out in offset order with the gaps between them
// left for the emitter to zero.
void Parser::flattenInit(const Type *type, Init &in, int base,
                         std::vector<GlobalPiece> &out) {
    if (type->isArray()) {
        const Type *elem = type->pointee();
        int step = elem->size(target_);

        if (const StrLit *s = stringInitialiser(in, type)) {
            const std::string &text = s->text();
            if (static_cast<long>(text.size()) > type->length())
                src_.fail(in.pos, "the string has " + std::to_string(text.size()) +
                                  " characters and the array holds " +
                                  std::to_string(type->length()));
            for (std::size_t i = 0; i < text.size(); i++)
                out.push_back(GlobalPiece{ base + static_cast<int>(i), 1,
                                           static_cast<long>(
                                               static_cast<unsigned char>(text[i])) });
            return;   // the rest of the array is a gap, and a gap is zero
        }

        if (!in.isList)
            src_.fail(in.pos, "an array at file scope needs a braced initialiser");
        if (static_cast<long>(in.items.size()) > type->length())
            src_.fail(in.pos, "the array has " + std::to_string(type->length()) +
                              " elements and its initialiser has " +
                              std::to_string(in.items.size()));
        for (std::size_t i = 0; i < in.items.size(); i++)
            flattenInit(elem, in.items[i], base + static_cast<int>(i) * step, out);
        return;
    }

    if (type->isStructOrUnion()) {
        if (!in.isList)
            src_.fail(in.pos, "a struct or union at file scope needs a braced "
                              "initialiser");
        const std::vector<Member> &members = type->members();
        std::size_t count = type->kind() == Kind::Union
                          ? (members.empty() ? std::size_t(0) : std::size_t(1))
                          : members.size();
        if (in.items.size() > count)
            src_.fail(in.pos, "it takes " + std::to_string(count) +
                              " initialiser(s) and was given " +
                              std::to_string(in.items.size()));
        for (std::size_t i = 0; i < in.items.size() && i < count; i++) {
            const Member &m = members[i];
            // A bit-field's initial value would have to be packed into a
            // storage unit shared with its neighbours, and the pieces here are
            // whole bytes. Refused by name rather than emitted wrongly.
            if (m.isBitField())
                src_.fail(in.items[i].pos,
                          "a bit-field cannot be initialised at file scope yet - "
                          "assign to it in a function");
            flattenInit(m.type, in.items[i], base + m.offset, out);
        }
        return;
    }

    if (in.isList) {
        if (in.items.size() != 1)
            src_.fail(in.pos, "this is not an aggregate and takes one value");
        flattenInit(type, in.items[0], base, out);
        return;
    }

    ExprPtr value = decay(std::move(in.value));

    // A floating member laid out before the program runs is its bit pattern,
    // written as an integer of the same width. There is nowhere to compute it
    // at run time, so the constant has to be turned into bytes here.
    if (type->isFloating()) {
        double d;
        if (!foldDouble(*value, &d))
            src_.fail(in.pos, "expected a constant initialiser, and this is not "
                              "a constant");
        long bits = 0;
        if (type->kind() == Kind::Float) {
            float f = static_cast<float>(d);
            unsigned int u;
            std::memcpy(&u, &f, sizeof u);
            bits = static_cast<long>(u);
        } else {
            unsigned long u;
            std::memcpy(&u, &d, sizeof u);
            bits = static_cast<long>(u);
        }
        out.push_back(GlobalPiece{ base, type->size(target_), bits });
        return;
    }

    long v;
    if (!fold(*value, &v, in.pos))
        src_.fail(in.pos, "expected a constant initialiser, and this is not an "
                          "integer constant expression");
    if (type->isInteger()) v = narrowTo(v, type);
    out.push_back(GlobalPiece{ base, type->size(target_), v });
}

ExprPtr Parser::objectRef(const std::string &name) {
    if (const Local *l = findLocal(name)) {
        // A static local reads and writes its data-section symbol. It is a
        // global everywhere except in who is allowed to say its name.
        Var *v = l->staticName.empty() ? Var::local(name, l->offset)
                                       : Var::global(l->staticName);
        v->setReadOnly(l->isConst);
        ExprPtr n(v);
        n->setType(l->type);
        return n;
    }
    if (const GlobalSym *g = findGlobal(name)) {
        Var *v = Var::global(name);
        v->setReadOnly(g->isConst);
        ExprPtr n(v);
        n->setType(g->type);
        return n;
    }
    return nullptr;
}

// Everything a call does once the callee is known. Both a call by name and a
// call through a pointer end here, so the argument rules cannot drift apart
// between them - which they would, since the second was added years after the
// first and nobody would have thought to change two places.
ExprPtr Parser::finishCall(const std::string &name, ExprPtr callee,
                           const Type *returns,
                           const std::vector<const Type *> &params,
                           bool variadic, std::size_t pos) {
    std::vector<ExprPtr> args;
    if (!consume(")")) {
        for (;;) {
            // assign(), not expr(): the commas here separate arguments and are
            // not operators. This is the distinction C draws by calling an
            // argument an assignment-expression.
            args.push_back(decay(assign()));
            if (consume(")")) break;
            expect(",");
        }
    }

    if (variadic ? args.size() < params.size() : args.size() != params.size())
        src_.fail(pos, "'" + name + "' takes " + (variadic ? "at least " : "") +
                       std::to_string(params.size()) + " argument(s), given " +
                       std::to_string(args.size()));

    // Named parameters convert as if by assignment - checked, then converted,
    // because "as if by assignment" carries assignment's constraints and not
    // only its conversions. The arguments past a variadic's named ones take the
    // default argument promotions instead, and there is nothing to check them
    // against: the prototype stopped describing them at the '...'.
    for (std::size_t i = 0; i < args.size(); i++) {
        if (i >= params.size()) {
            args[i] = defaultPromote(std::move(args[i]));
            continue;
        }
        checkAssignable(*args[i], params[i], pos,
                        "argument " + std::to_string(i + 1) + " of '" + name + "'");
        args[i] = convert(std::move(args[i]), params[i]);
    }

    // The register limit is System V's, not the parser's, but it is caught here
    // because here there is a line to point at. Code generation could only say
    // that something, somewhere, had too many arguments.
    // A struct spends one register per eightbyte, and which file each comes
    // from is System V's classification rather than the struct's own type.
    int ints = 0, sses = 0;
    for (const ExprPtr &a : args) {
        if (a->type()->isStructOrUnion()) {
            for (bool sse : classifyEightbytes(a->type(), target_))
                if (sse) sses++; else ints++;
        } else if (a->type()->isFloating()) {
            sses++;
        } else {
            ints++;
        }
    }
    if (ints > kMaxArgs)
        src_.fail(pos, "'" + name + "' is called with " + std::to_string(ints) +
                       " integer arguments; only " + std::to_string(kMaxArgs) +
                       " fit in registers and the rest would go on the stack, "
                       "which is not supported yet");
    if (sses > 8)
        src_.fail(pos, "'" + name + "' is called with " + std::to_string(sses) +
                       " floating arguments; only 8 fit in registers");

    // Somewhere for a returned struct to live. Allocated even when the value is
    // discarded, which costs a few bytes of frame and keeps the rule simple.
    int slot = returns->isStructOrUnion() ? allocateFrameSlot(returns) : 0;
    ExprPtr n(new Call(name, std::move(callee), std::move(args), variadic, slot));
    n->setType(returns);
    return n;
}

// a[i] is defined as *(a + i). Building it that way rather than as its own node
// is why i[a] also works, which is legal C however strange it looks.
ExprPtr Parser::postfix() {
    ExprPtr n = primary(current_);
    for (;;) {
        std::size_t pos = peek().pos;

        // A call through anything that holds a pointer to a function. Written
        // here rather than beside the call-by-name so that "f(1)",
        // "table[i](1)" and "s.op(1)" are one rule: whatever the postfix chain
        // produced, if it is a pointer to a function then a '(' calls it.
        //
        // The name in the message is the type, since the expression has none -
        // "'int (*)(int, int)' takes 2 argument(s), given 3" says as much as a
        // name would have.
        if (peek().is("(") && n->type()->isFunctionPointer()) {
            at_++;
            const Type *fn = n->type()->pointee();
            // Read out of n before it is moved from. As arguments to one call
            // these would be evaluated in an unspecified order, and the order
            // that moves first leaves the other dereferencing nothing.
            std::string called = n->type()->describe();
            n = finishCall(called, std::move(n), fn->returns(), fn->params(),
                           fn->isVariadicFn(), pos);
            continue;
        }

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
            ExprPtr acc(new MemberAccess(std::move(n), name, m->offset,
                                         m->width, m->bitOffset));
            acc->setType(m->type);
            n = std::move(acc);
            continue;
        }

        if (peek().is("++") || peek().is("--")) {
            bool up = peek().is("++");
            at_++;
            n = incDec(std::move(n), up, false, pos);
            continue;
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
            ExprPtr acc(new MemberAccess(std::move(n), name, m->offset,
                                         m->width, m->bitOffset));
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
        // The one place the compiler's own invariant and C's rule are the same
        // rule. A bit-field does not begin at an address, so there is nothing
        // for this to yield - and genAddr, which everything else here uses to
        // find a place, would have nothing to answer either.
        if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(v.get()))
            if (m->isBitField())
                src_.fail(pos, "'" + m->name() + "' is a bit-field, and a "
                               "bit-field has no address");
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
            // An abstract declarator: no name, but every shape a named one can
            // take. "sizeof(char[8])" and "sizeof(int (*)[4])" are this.
            measured = declarator(measured, true).type;
            expect(")");
        } else {
            // Not decayed: sizeof of an array is the array's own size, which is
            // the whole reason decay has exceptions.
            ExprPtr operand = unary();
            // A bit-field has no size of its own either - it shares a storage
            // unit, and the width it was declared with is not a number of bytes.
            if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(operand.get()))
                if (m->isBitField())
                    src_.fail(pos, "sizeof cannot be applied to '" + m->name() +
                                   "', which is a bit-field");
            measured = operand->type();
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
            // The same abstract declarator, which is what lets a malloc'd block
            // be cast to "(int (*)[4])" - a pointer to rows of four.
            to = declarator(to, true).type;
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
        ExprPtr n(new MemberAccess(std::move(obj), m->name(), m->offset(),
                                   m->width(), m->bitOffset()));
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
    const Type *rt = promote(rhs->type());       // before the move, as above
    ExprPtr n(new Binary(op, convert(std::move(lhs), lt),
                             convert(std::move(rhs), rt)));
    n->setType(lt);
    return n;
}

// x op= e becomes x = x op e. The target appears twice in the tree, which is
// correct while evaluating it has no side effect - the parser refuses a target
// that is anything but a name, a member or a dereference, and none of those
// can change anything by being evaluated.
// Every write through an lvalue comes here first. One place, so that a const
// object cannot be reached by a route that forgot to look - '=' and "+=" and
// "++" are three spellings of the same store.
void Parser::requireAssignable(const Expr &e, std::size_t pos, const char *what) {
    if (!isLvalue(e))
        src_.fail(pos, std::string(what) + " is not something that can be assigned to");
    if (e.type()->isArray())
        src_.fail(pos, "an array cannot be assigned to");
    if (const Var *v = dynamic_cast<const Var *>(&e))
        if (v->readOnly())
            src_.fail(pos, "'" + v->name() + "' is const and cannot be assigned to");
}

ExprPtr Parser::compound(BinOp op, ExprPtr target, ExprPtr value, std::size_t pos) {
    requireAssignable(*target, pos, "the left of a compound assignment");
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
    // The prefix form is "x = x + 1" and nothing more, so it borrows compound
    // assignment whole - including its const check and its bit-field path.
    if (prefix) {
        ExprPtr one(new Num(1L));
        one->setType(types_.intType());
        return compound(increment ? BinOp::Add : BinOp::Sub, std::move(target),
                        std::move(one), pos);
    }

    // The postfix form cannot borrow it. Its value is what the object held
    // before the store, and "(x += 1) - 1" is wrong wherever the type wraps:
    // an unsigned char at 255 yields 255 and stores 0, while that rewrite
    // computes 0 - 1. So it is a node, and code generation keeps the old value
    // on the stack across the store.
    const char *what = increment ? "the operand of postfix '++'"
                                 : "the operand of postfix '--'";
    requireAssignable(*target, pos, what);
    const Type *t = target->type();
    if (!t->isScalar())
        src_.fail(pos, std::string(what) + " needs a number or a pointer, not '" +
                       t->describe() + "'");
    if (const MemberAccess *m = dynamic_cast<const MemberAccess *>(target.get()))
        if (m->isBitField())
            src_.fail(pos, "postfix '++' and '--' on a bit-field are not supported "
                           "yet - the prefix form works, and so does 'f.a = f.a + 1'");
    if (t->isPointer() && !t->pointee()->isComplete())
        src_.fail(pos, std::string(what) + " is '" + t->describe() +
                       "', and there is no size to step by");

    // How far one step moves: an element for a pointer, one for everything else.
    long step = t->isPointer() ? t->pointee()->size(target_) : 1;
    ExprPtr n(new Postfix(std::move(target), increment, step));
    n->setType(t);
    return n;
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

    requireAssignable(*n, pos, "the left of '='");

    const Type *to = n->type();
    // Checked, then converted. This is the assignment the constraints are
    // named after; the call site and the initialiser below borrow it.
    ExprPtr value = decay(assign());
    checkAssignable(*value, to, pos, "the left of '='");
    ExprPtr node(new Assign(std::move(n), convert(std::move(value), to)));
    node->setType(to);
    return node;
}

// The comma operator, and the top of the expression grammar.
//
// Everything that wants one expression and no commas calls assign() instead,
// which is what C means by "assignment-expression": a call argument and an
// initialiser both do, or "f(a, b)" would be one argument and "int x = a, y"
// would initialise x with a comma expression rather than declaring y.
ExprPtr Parser::expr() {
    ExprPtr n = assign();
    while (consume(",")) {
        ExprPtr right = decay(assign());
        const Type *t = right->type();
        ExprPtr c(new Comma(std::move(n), std::move(right)));
        c->setType(t);
        n = std::move(c);
    }
    return n;
}

// ---- statements ----

StmtPtr Parser::declaration() {
    StorageClass sc;
    Qualifiers quals;
    const Type *base = specifiers(&sc, &quals);

    // "struct Point { int x; int y; };" declares a type and nothing else.
    if (peek().is(";")) { at_++; return StmtPtr(new Block({})); }

    if (sc == StorageTypedef) {
        do {
            Declared td = declarator(base);
            if (findTypedef(td.name)) src_.fail(td.pos, "'" + td.name + "' is typedefed twice");
            typedefIndex_[td.name] = typedefs_.size();
            typedefs_.push_back(TypedefName{ td.name, td.type });
        } while (consume(","));
        expect(";");
        return StmtPtr(new Block({}));
    }
    if (sc == StorageExtern)
        src_.fail(peek().pos, "'extern' on a local is not supported yet - "
                              "declare it at file scope");

    // One declaration may declare several names, each with its own declarator
    // and its own initialiser: "int x, *p = &x, a[4];". The specifiers are
    // shared and everything after them is not, which is why the '*' and the
    // '[4]' belong to one name apiece.
    //
    // The initialisers become statements in a Block, which introduces no scope
    // of its own here - the names were entered into the enclosing one by
    // declare() as each declarator was read, so they are visible to the
    // initialisers that follow them, as C requires for "int x = 1, y = x;".
    std::vector<StmtPtr> inits;
    do {
        Declared d = declarator(base);
        // "int a[] = {1,2,3}" declares an incomplete type on purpose, and its
        // initialiser is what completes it. That is the only case, so the check
        // waits to see whether an '=' follows.
        bool sizedByInitialiser = d.type->isArray() && d.type->length() < 0 &&
                                  peek().is("=");
        if (!d.type->isComplete() && !sizedByInitialiser)
            src_.fail(d.pos, "'" + d.name + "' has an incomplete type");

        // A static local is a global that only this block can name. Its
        // initialiser runs once, before the program does, which is why it must
        // be a constant and why no statement is produced for it - the value is
        // in the data section, not in an assignment at the top of the function.
        if (sc == StorageStatic) {
            // Two blocks in one function may each declare "static int n", and
            // they are two objects. The name alone would give them one symbol,
            // and the assembler says so - which is how this was found.
            std::string symbol = functionName_ + "." + d.name;
            for (int n = 1; ; n++) {
                bool taken = false;
                for (const std::string &used : staticSymbols_)
                    if (used == symbol) { taken = true; break; }
                if (!taken) break;
                symbol = functionName_ + "." + d.name + "." + std::to_string(n);
            }
            staticSymbols_.push_back(symbol);
            // A static local lives in the data section, so its initialiser is
            // data and not statements - the same treatment a file-scope object
            // gets, and for the same reason: it is laid out before the program
            // runs.
            std::vector<GlobalPiece> pieces;
            bool hasInit = false;
            if (consume("=")) {
                Init in = parseInitialiser();
                if (d.type->isArray() && d.type->length() < 0)
                    d.type = types_.arrayOf(d.type->pointee(),
                                            inferredLength(in, d.type->pointee(), d.pos));
                flattenInit(d.type, in, 0, pieces);
                hasInit = true;
            } else if (d.type->isArray() && d.type->length() < 0) {
                src_.fail(d.pos, "'" + d.name + "' has no length and no initialiser "
                                 "to take one from");
            }
            declareStaticLocal(d.name, d.type, d.pos, symbol);
            locals_.back().isConst = quals.isConst;
            current_->globals.push_back(Global{ symbol, d.type, std::move(pieces),
                                                hasInit, true });
            continue;
        }

        // "int a[] = {1,2,3}" has no length until its initialiser has been
        // counted, so the initialiser is read before the object is declared -
        // which is also why the parse produces an Init rather than statements
        // straight away.
        bool hasInit = peek().is("=");
        Init in;
        if (hasInit) {
            at_++;
            in = parseInitialiser();
            if (d.type->isArray() && d.type->length() < 0)
                d.type = types_.arrayOf(d.type->pointee(),
                                        inferredLength(in, d.type->pointee(), d.pos));
        } else if (d.type->isArray() && d.type->length() < 0) {
            src_.fail(d.pos, "'" + d.name + "' has no length and no initialiser "
                             "to take one from");
        }

        declare(d.name, d.type, d.pos);
        locals_.back().isConst = quals.isConst;

        if (hasInit) {
            // An initialiser is an assignment written with the declaration, and
            // C constrains it the same way - "char *p = 5;" is the same mistake
            // as "char *p; p = 5;" and gets the same message. An aggregate is
            // that rule applied to each piece in turn.
            std::vector<InitStep> path;
            emitInit(d.name, path, d.type, in, inits);
        }
    } while (consume(","));

    expect(";");
    return StmtPtr(new Block(std::move(inits)));
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
// An integer constant expression.
//
// Parsed as an ordinary conditional expression and then folded, rather than by
// a second small grammar that reads a number and a minus sign. Parsing it as an
// expression means the type checker has already run over it: the promotions and
// the conversions are Cast nodes in the tree before the folder sees anything,
// so "case 'a' + 1" and "int a[sizeof(int) * 2]" need no rule of their own.
//
// Four sites use this - a case value, an enumerator, a global's initialiser, an
// array length - and each of them had its own "a constant only" refusal before
// it existed. They were all refusing the same thing.
long Parser::constantExpression(const char *what) {
    std::size_t pos = peek().pos;
    ExprPtr e = decay(conditional());
    long v;
    if (!fold(*e, &v, pos))
        src_.fail(pos, std::string("expected ") + what +
                       ", and this is not an integer constant expression");
    return v;
}

// The folder. Everything it does not recognise is not a constant, which is the
// safe direction to be wrong in: a missed fold is a refusal with a message,
// never a wrong number.
bool Parser::fold(const Expr &e, long *out, std::size_t pos) const {
    if (const Num *n = dynamic_cast<const Num *>(&e)) {
        // 1.5 is a constant and is not an integer one. C is specific about
        // this, and so is the message the caller gives.
        if (n->type()->isFloating()) return false;
        *out = n->value();
        return true;
    }

    // Casts are where the width and the signedness of the answer are decided,
    // because the parser has already put one at every point C converts.
    if (const Cast *c = dynamic_cast<const Cast *>(&e)) {
        long v;
        if (!fold(c->value(), &v, pos)) return false;
        if (!e.type()->isInteger()) return false;
        *out = narrowTo(v, e.type());
        return true;
    }

    if (const Unary *u = dynamic_cast<const Unary *>(&e)) {
        long v;
        if (!fold(u->operand(), &v, pos)) return false;
        switch (u->op()) {
        case '-': *out = static_cast<long>(0UL - static_cast<unsigned long>(v)); return true;
        case '+': *out = v; return true;
        case '!': *out = !v; return true;
        case '~': *out = ~v; return true;
        default: return false;      // '*' reads storage and '&' names it
        }
    }

    // Only the arm that is taken has to be constant, which is what makes
    // "sizeof(long) == 8 ? 64 : 32" work as the idiom it is.
    if (const Conditional *c = dynamic_cast<const Conditional *>(&e)) {
        long t;
        if (!fold(c->cond(), &t, pos)) return false;
        return fold(t ? c->thenArm() : c->elseArm(), out, pos);
    }

    if (const Binary *b = dynamic_cast<const Binary *>(&e)) {
        long l, r;
        if (!fold(b->lhs(), &l, pos) || !fold(b->rhs(), &r, pos)) return false;

        // Signedness picks the operation here exactly as it picks the
        // instruction in code generation: -1 / 2u is an enormous number and
        // -1 < 1u is false. Both operands were converted to one type by the
        // parser, so either side answers the question.
        const Type *t = b->lhs().type();
        bool uns = t->isInteger() && !t->isSigned(target_);
        unsigned long ul = static_cast<unsigned long>(l);
        unsigned long ur = static_cast<unsigned long>(r);

        // Arithmetic runs through unsigned long and comes back, so that an
        // overflow in the folder wraps like the machine rather than being
        // undefined behaviour inside the compiler itself.
        switch (b->op()) {
        case BinOp::Add: *out = static_cast<long>(ul + ur); return true;
        case BinOp::Sub: *out = static_cast<long>(ul - ur); return true;
        case BinOp::Mul: *out = static_cast<long>(ul * ur); return true;
        case BinOp::Div:
        case BinOp::Mod:
            if (r == 0)
                src_.fail(pos, "division by zero in a constant expression");
            // The one signed division that overflows. Left as it is rather
            // than trapping, which is what the hardware would do to it.
            if (!uns && ul == (1UL << 63) && r == -1) {
                *out = (b->op() == BinOp::Div) ? l : 0;
                return true;
            }
            if (b->op() == BinOp::Div)
                *out = uns ? static_cast<long>(ul / ur) : l / r;
            else
                *out = uns ? static_cast<long>(ul % ur) : l % r;
            return true;
        case BinOp::Shl:
        case BinOp::Shr:
            if (r < 0 || r >= 64)
                src_.fail(pos, "shift count out of range in a constant expression");
            if (b->op() == BinOp::Shl) *out = static_cast<long>(ul << r);
            else *out = uns ? static_cast<long>(ul >> r) : (l >> r);
            return true;
        case BinOp::BitAnd: *out = l & r; return true;
        case BinOp::BitOr:  *out = l | r; return true;
        case BinOp::BitXor: *out = l ^ r; return true;
        case BinOp::Eq: *out = (l == r); return true;
        case BinOp::Ne: *out = (l != r); return true;
        case BinOp::Lt: *out = uns ? (ul <  ur) : (l <  r); return true;
        case BinOp::Le: *out = uns ? (ul <= ur) : (l <= r); return true;
        case BinOp::Gt: *out = uns ? (ul >  ur) : (l >  r); return true;
        case BinOp::Ge: *out = uns ? (ul >= ur) : (l >= r); return true;
        case BinOp::LAnd: *out = (l && r); return true;
        case BinOp::LOr:  *out = (l || r); return true;
        }
        return false;
    }

    // A variable, a call, an address, a string: all constant in the loose sense
    // and none of them an integer constant expression.
    return false;
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
        value = narrowTo(constantExpression("a case value"),
                         switches_.back().governing);
        for (const Case *c : switches_.back().cases)
            if (c->value() == value)
                src_.fail(pos, "duplicate case value " + std::to_string(value));
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
        std::size_t pos = peek().pos;
        ExprPtr value = decay(expr());
        // C says a return converts as if by assignment to the function's type,
        // which is the third and last place that phrase appears.
        checkAssignable(*value, returnType_, pos, "this function's return type");
        value = convert(std::move(value), returnType_);
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
    Qualifiers quals;
    const Type *base = specifiers(&sc, &quals);

    // A type declaration on its own, with no object: "struct Point { ... };"
    if (peek().is(";")) { at_++; return; }

    if (sc == StorageTypedef) {
        do {
            Declared td = declarator(base);
            if (findTypedef(td.name)) src_.fail(td.pos, "'" + td.name + "' is typedefed twice");
            typedefIndex_[td.name] = typedefs_.size();
            typedefs_.push_back(TypedefName{ td.name, td.type });
        } while (consume(","));
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

    // An object, and possibly several: "int g, h = 4;". The first declarator is
    // already read, so the loop is entered from the middle - which is the price
    // of not knowing whether this was a function until the '(' failed to
    // appear.
    if (!peek().is("(")) {
        for (;;) {
            if (d.type->isVoid()) src_.fail(d.pos, "'" + d.name + "' cannot have type void");

            std::vector<GlobalPiece> pieces;
            bool hasInit = false;
            if (consume("=")) {
                Init in = parseInitialiser();
                if (d.type->isArray() && d.type->length() < 0)
                    d.type = types_.arrayOf(d.type->pointee(),
                                            inferredLength(in, d.type->pointee(), d.pos));
                flattenInit(d.type, in, 0, pieces);
                hasInit = true;
            } else if (d.type->isArray() && d.type->length() < 0 &&
                       sc != StorageExtern) {
                // "extern int table[];" is the exception and a common one: the
                // length lives in the unit that defines the array, and this
                // declaration exists precisely so the others need not know it.
                src_.fail(d.pos, "'" + d.name + "' has no length and no initialiser "
                                 "to take one from");
            }

            // Declared before. C allows that as often as one likes, provided
            // the type agrees and only one declaration initialises it - which
            // is the whole mechanism a header runs on: it says "extern int x;"
            // to every unit, including the one that then writes "int x = 0;".
            // Refusing the second was wrong, and only a test that compiles two
            // units against one header could show it.
            if (GlobalSym *prev = findGlobalToUpdate(d.name)) {
                if (prev->type != d.type)
                    src_.fail(d.pos, "'" + d.name + "' was already declared as '" +
                                     prev->type->describe() + "', not '" +
                                     d.type->describe() + "'");
                if (hasInit && prev->hasInit)
                    src_.fail(d.pos, "'" + d.name + "' is given an initialiser twice");
                if (hasInit) prev->hasInit = true;

                if (sc != StorageExtern) {
                    if (!prev->emitted) {
                        prev->emitted = true;
                        program.globals.push_back(Global{ d.name, d.type, pieces,
                                                          hasInit, sc == StorageStatic });
                    } else if (hasInit) {
                        // Storage went out already, with a zero, because the
                        // earlier declaration had no initialiser. This is the
                        // value it was waiting for.
                        for (Global &g : program.globals)
                            if (g.name == d.name) { g.init = pieces; g.hasInit = true; break; }
                    }
                }
                if (!consume(",")) break;
                d = declarator(base);
                continue;
            }

            globalIndex_[d.name] = globals_.size();
            globals_.push_back(GlobalSym{ d.name, d.type, quals.isConst,
                                          sc != StorageExtern, hasInit });
            // extern says the object lives in another unit, so nothing is
            // emitted for it.
            if (sc != StorageExtern)
                program.globals.push_back(Global{ d.name, d.type, std::move(pieces),
                                                  hasInit, sc == StorageStatic });
            if (!consume(",")) break;
            d = declarator(base);
        }
        expect(";");
        return;
    }

    expect("(");
    std::vector<const Type *> params;
    std::vector<Param> paramSlots;
    bool variadic = false;
    // Where an unnamed parameter was seen, if one was. Legal in a prototype and
    // not in a definition - a body cannot use what it cannot name, and C says so.
    std::size_t unnamedParam = 0;
    bool sawUnnamed = false;

    if (!consume(")")) {
        if (peek().is("void") && peekAt(1).is(")")) {
            at_ += 2;
        } else {
            for (;;) {
                if (consume("...")) { variadic = true; expect(")"); break; }
                StorageClass psc;
                Qualifiers pquals;
                const Type *pt = specifiers(&psc, &pquals);
                Declared pd = declarator(pt, true);
                // A parameter declared as an array is a pointer. That rule is
                // why sizeof inside a function gives 8 for a char[16] parameter.
                if (pd.type->isArray())
                    pd.type = types_.pointerTo(pd.type->pointee());
                int off;
                if (pd.name.empty()) {
                    // Nothing is entered in the symbol table, and no frame slot
                    // is taken either. An unnamed parameter can only occur in a
                    // prototype - a definition with one is refused below - and a
                    // prototype emits no prologue, so there is no store for a
                    // slot to be the destination of. The offset is recorded as
                    // zero because nothing will ever read it.
                    if (pd.type->isVoid())
                        src_.fail(pd.pos, "'void' is only a parameter list on its own");
                    unnamedParam = pd.pos;
                    sawUnnamed = true;
                    off = 0;
                } else {
                    off = declare(pd.name, pd.type, pd.pos);
                    // "const int n" makes the parameter itself read-only. "const
                    // char *s" does not - there the qualifier belongs to what s
                    // points at, which this model does not carry.
                    locals_.back().isConst = pquals.isConst;
                }
                params.push_back(pd.type);
                paramSlots.push_back(Param{ pd.type, off });
                if (consume(")")) break;
                expect(",");
            }
        }
    }
    // System V hands a small struct over in registers and a large one in
    // memory, and only the first of those is written here. A struct of more
    // than two eightbytes is MEMORY class: the caller would copy it onto the
    // stack, and this compiler has no stack arguments at all - which is the
    // same reason it stops at six parameters.
    for (const Type *pt : params)
        if (pt->isStructOrUnion() && pt->size(target_) > 16)
            src_.fail(d.pos, "passing a '" + pt->describe() + "' by value is not "
                             "supported yet - it is " + std::to_string(pt->size(target_)) +
                             " bytes, and anything over 16 goes on the stack; "
                             "pass a pointer to it");
    if (d.type->isStructOrUnion() && d.type->size(target_) > 16)
        src_.fail(d.pos, "returning a '" + d.type->describe() + "' by value is not "
                         "supported yet - it is " + std::to_string(d.type->size(target_)) +
                         " bytes, and anything over 16 is returned through a "
                         "hidden pointer this compiler does not pass");
    if (static_cast<int>(params.size()) > kMaxArgs)
        src_.fail(d.pos, "more than " + std::to_string(kMaxArgs) +
                         " parameters is not supported yet");

    if (consume(";")) {
        declareFunction(d.name, d.type, params, variadic, false, d.pos);
        return;
    }
    if (variadic)
        src_.fail(d.pos, "defining a variadic function is not supported yet");

    if (sawUnnamed)
        src_.fail(unnamedParam, "a parameter of a definition needs a name - "
                                "a prototype may leave it out, a body cannot");

    declareFunction(d.name, d.type, params, variadic, true, d.pos);
    returnType_ = d.type;
    functionName_ = d.name;
    staticSymbols_.clear();

    StmtPtr body = block();
    resolveGotos();

    int frame = alignTo(frameSize_, 16);
    program.functions.push_back(Function(d.name, d.type, std::move(paramSlots),
                                         std::move(body), frame,
                                         sc == StorageStatic));
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
