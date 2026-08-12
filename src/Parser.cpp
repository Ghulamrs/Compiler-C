#include "Parser.h"
#include "Source.h"

// Six is not a parser limit, it is the System V one: integer arguments beyond
// the sixth go on the stack, which is a different lowering and a separate
// change. Refusing here gives a message about C; letting it through would give
// a program that calls with garbage.
static const int kMaxArgs = 6;

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

// ---- the two symbol tables ----

// Every local is eight bytes below %rbp. Offsets grow downwards as declarations
// are met, so the frame size is the last offset handed out. Parameters are
// declared first, which is what lets the prologue spill the argument registers
// into slots -8, -16, ... without being told where they went.
int Parser::declare(const std::string &name, std::size_t pos) {
    for (const Local &l : locals_)
        if (l.name == name)
            src_.fail(pos, "'" + name + "' is declared twice");
    frameSize_ += 8;
    locals_.push_back(Local{ name, frameSize_ });
    return frameSize_;
}

int Parser::lookup(const std::string &name, std::size_t pos) const {
    for (const Local &l : locals_)
        if (l.name == name) return l.offset;
    src_.fail(pos, "'" + name + "' was not declared");
}

// A prototype may be repeated, and a definition may follow one, but the two
// have to agree. Disagreeing is caught here rather than at the call, because
// the call is not the thing that is wrong.
void Parser::declareFunction(const std::string &name, int params, bool defining,
                             std::size_t pos) {
    for (Signature &f : functions_) {
        if (f.name != name) continue;
        if (f.params != params)
            src_.fail(pos, "'" + name + "' was declared with " +
                           std::to_string(f.params) + " parameter(s), and this says " +
                           std::to_string(params));
        if (defining) {
            if (f.defined)
                src_.fail(pos, "'" + name + "' is defined twice");
            f.defined = true;
        }
        return;
    }
    functions_.push_back(Signature{ name, params, defining, pos });
}

const Parser::Signature &Parser::lookupFunction(const std::string &name,
                                                std::size_t pos) const {
    for (const Signature &f : functions_)
        if (f.name == name) return f;
    src_.fail(pos, "'" + name + "' was not declared - a prototype must come first");
}

// ---- expressions ----

ExprPtr Parser::primary() {
    if (consume("(")) {
        ExprPtr e = expr();
        expect(")");
        return e;
    }
    if (peek().kind == TokenKind::Num) {
        long v = peek().value;
        at_++;
        return ExprPtr(new Num(v));
    }
    if (peek().kind == TokenKind::Ident) {
        std::string name = peek().text;
        std::size_t pos = peek().pos;

        // An identifier followed by '(' is a call; anything else is a variable.
        // One token of lookahead is all that is needed to tell them apart.
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

            // The whole reason a prototype has to come first.
            const Signature &sig = lookupFunction(name, pos);
            int given = static_cast<int>(args.size());
            if (given != sig.params)
                src_.fail(pos, "'" + name + "' takes " + std::to_string(sig.params) +
                               " argument(s), given " + std::to_string(given));

            return ExprPtr(new Call(name, std::move(args)));
        }

        at_++;
        return ExprPtr(new Var(name, lookup(name, pos)));
    }
    src_.fail(peek().pos, "expected an expression");
}

// Unary plus is a no-op to the machine but not to the parser: it has to be
// consumed, or "+3" fails at primary.
ExprPtr Parser::unary() {
    if (consume("+")) return unary();
    if (consume("-")) return ExprPtr(new Unary('-', unary()));
    return primary();
}

ExprPtr Parser::mul() {
    ExprPtr n = unary();
    for (;;) {
        if (consume("*"))      n = ExprPtr(new Binary(BinOp::Mul, std::move(n), unary()));
        else if (consume("/")) n = ExprPtr(new Binary(BinOp::Div, std::move(n), unary()));
        else if (consume("%")) n = ExprPtr(new Binary(BinOp::Mod, std::move(n), unary()));
        else return n;
    }
}

ExprPtr Parser::add() {
    ExprPtr n = mul();
    for (;;) {
        if (consume("+"))      n = ExprPtr(new Binary(BinOp::Add, std::move(n), mul()));
        else if (consume("-")) n = ExprPtr(new Binary(BinOp::Sub, std::move(n), mul()));
        else return n;
    }
}

ExprPtr Parser::relational() {
    ExprPtr n = add();
    for (;;) {
        if (consume("<"))       n = ExprPtr(new Binary(BinOp::Lt, std::move(n), add()));
        else if (consume("<=")) n = ExprPtr(new Binary(BinOp::Le, std::move(n), add()));
        else if (consume(">"))  n = ExprPtr(new Binary(BinOp::Gt, std::move(n), add()));
        else if (consume(">=")) n = ExprPtr(new Binary(BinOp::Ge, std::move(n), add()));
        else return n;
    }
}

ExprPtr Parser::equality() {
    ExprPtr n = relational();
    for (;;) {
        if (consume("=="))      n = ExprPtr(new Binary(BinOp::Eq, std::move(n), relational()));
        else if (consume("!=")) n = ExprPtr(new Binary(BinOp::Ne, std::move(n), relational()));
        else return n;
    }
}

// Recursion rather than a loop, because a = b = c groups to the right. The
// left side is re-read as a name: only a simple local is assignable today, so
// anything else is rejected here rather than storing to nowhere.
ExprPtr Parser::assign() {
    std::size_t mark = at_;
    ExprPtr n = equality();
    if (!peek().is("=")) return n;

    const Token &target = tokens_[mark];
    if (target.kind != TokenKind::Ident || at_ != mark + 1)
        src_.fail(peek().pos, "left of '=' is not something that can be assigned to");
    at_++;  // the '='
    return ExprPtr(new Assign(target.text, lookup(target.text, target.pos), assign()));
}

ExprPtr Parser::expr() { return assign(); }

// ---- statements ----

StmtPtr Parser::declaration() {
    expect("int");
    std::size_t pos = peek().pos;
    std::string name = expectIdent("a name after 'int'");

    int offset = declare(name, pos);

    // A declaration with an initialiser is a declaration and an assignment.
    // Making the slot first and lowering the rest to a normal Assign keeps one
    // path to a store instead of two that must agree.
    if (consume("=")) {
        ExprPtr value = expr();
        expect(";");
        return StmtPtr(new ExprStmt(ExprPtr(new Assign(name, offset, std::move(value)))));
    }
    expect(";");
    return StmtPtr(new Block({}));   // a bare declaration emits nothing
}

StmtPtr Parser::block() {
    expect("{");
    std::vector<StmtPtr> body;
    while (!peek().is("}")) {
        if (peek().kind == TokenKind::End)
            src_.fail(peek().pos, "unclosed '{'");
        body.push_back(peek().is("int") ? declaration() : statement());
    }
    expect("}");
    return StmtPtr(new Block(std::move(body)));
}

StmtPtr Parser::statement() {
    if (consume("return")) {
        ExprPtr value = expr();
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

    if (consume(";")) return StmtPtr(new Block({}));   // the empty statement

    ExprPtr e = expr();
    expect(";");
    return StmtPtr(new ExprStmt(std::move(e)));
}

// ---- functions ----

// Shared by a prototype and a definition, because the two say the same thing
// and only differ in what follows the ')'. Parameters become the first locals.
// "(void)" and "()" are both an empty list; the old-style "()" meaning
// "unspecified" is not a distinction this compiler makes.
int Parser::parameterList() {
    locals_.clear();
    frameSize_ = 0;
    int params = 0;

    if (consume(")")) return 0;
    if (consume("void")) { expect(")"); return 0; }

    for (;;) {
        expect("int");
        std::size_t pos = peek().pos;
        declare(expectIdent("a parameter name"), pos);
        params++;
        if (consume(")")) break;
        expect(",");
    }
    return params;
}

void Parser::functionOrPrototype(Program &program) {
    expect("int");
    std::size_t namePos = peek().pos;
    std::string name = expectIdent("a function name");
    expect("(");

    int params = parameterList();
    if (params > kMaxArgs)
        src_.fail(namePos, "more than " + std::to_string(kMaxArgs) +
                           " parameters is not supported yet");

    // ';' here and it was a promise; '{' and it is the thing itself.
    if (consume(";")) {
        declareFunction(name, params, false, namePos);
        return;
    }

    // Registered before the body is parsed, so a function may call itself.
    declareFunction(name, params, true, namePos);

    StmtPtr body = block();

    // Rounded up to sixteen: the ABI wants %rsp 16-byte aligned at a call, and
    // getting it wrong stays invisible until the first call into libc.
    int frame = (frameSize_ + 15) / 16 * 16;
    program.push_back(Function(std::move(name), params, std::move(body), frame));
}

Program Parser::parse() {
    Program program;
    while (peek().kind != TokenKind::End)
        functionOrPrototype(program);

    if (program.empty())
        src_.fail(0, "the file defines no functions");
    return program;
}
