#include "Parser.h"
#include "Source.h"

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

// Every local is eight bytes and sits below %rbp. Offsets grow downwards as
// declarations are met, so the frame size is simply the last offset handed out.
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
// left side is re-read as a Var: only a simple local is assignable today, so
// anything else is rejected here rather than producing a store to nowhere.
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
    if (peek().kind != TokenKind::Ident)
        src_.fail(peek().pos, "expected a name after 'int'");
    std::string name = peek().text;
    std::size_t pos = peek().pos;
    at_++;

    int offset = declare(name, pos);

    // A declaration with an initialiser is a declaration and an assignment.
    // Making the slot first and lowering the rest to a normal Assign keeps
    // one path to a store instead of two that must agree.
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

Function Parser::parse() {
    expect("int");
    if (!peek().is("main")) src_.fail(peek().pos, "only main() is supported yet");
    at_++;
    expect("(");
    consume("void");            // "()" and "(void)" both accepted
    expect(")");

    StmtPtr body = block();

    if (peek().kind != TokenKind::End)
        src_.fail(peek().pos, "trailing text after the end of main");

    // Rounded up to sixteen: the ABI wants %rsp 16-byte aligned at a call, and
    // getting that wrong is invisible until the first call into libc.
    int frame = (frameSize_ + 15) / 16 * 16;
    return Function(std::move(body), frame);
}
