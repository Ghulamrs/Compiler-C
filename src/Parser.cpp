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

NodePtr Parser::primary() {
    if (consume("(")) {
        NodePtr n = expr();
        expect(")");
        return n;
    }
    if (peek().kind == TokenKind::Num) {
        long v = peek().value;
        at_++;
        return NodePtr(new Num(v));
    }
    src_.fail(peek().pos, "expected an expression");
}

// Unary plus is a no-op to the machine but not to the parser: it has to be
// consumed, or "+3" fails at primary.
NodePtr Parser::unary() {
    if (consume("+")) return unary();
    if (consume("-")) return NodePtr(new Unary('-', unary()));
    return primary();
}

NodePtr Parser::mul() {
    NodePtr n = unary();
    for (;;) {
        if (consume("*"))      n = NodePtr(new Binary('*', std::move(n), unary()));
        else if (consume("/")) n = NodePtr(new Binary('/', std::move(n), unary()));
        else return n;
    }
}

NodePtr Parser::expr() {
    NodePtr n = mul();
    for (;;) {
        if (consume("+"))      n = NodePtr(new Binary('+', std::move(n), mul()));
        else if (consume("-")) n = NodePtr(new Binary('-', std::move(n), mul()));
        else return n;
    }
}

NodePtr Parser::parse() {
    expect("int");
    if (!peek().is("main")) src_.fail(peek().pos, "only main() is supported yet");
    at_++;
    expect("(");
    consume("void");            // "()" and "(void)" both accepted
    expect(")");
    expect("{");
    expect("return");

    NodePtr value = expr();

    expect(";");
    expect("}");
    if (peek().kind != TokenKind::End)
        src_.fail(peek().pos, "trailing text after '}'");

    return NodePtr(new Return(std::move(value)));
}
