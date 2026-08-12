// Parser.h - tokens to a tree, by recursive descent.
//
// Hand-written rather than generated. bison 3.7.4 is on this box and would do
// it, but a C grammar's hard parts - the declarator syntax, and the typedef
// ambiguity where (A)*b is a cast or a multiplication depending on what A was
// declared as - are exactly what a generator does not solve for you. Those
// need the parser to consult a symbol table as it goes, which is why the scope
// stack will live in this class rather than in a later pass.
//
// The grammar accepted today:
//
//   program = "int" "main" "(" ["void"] ")" "{" "return" expr ";" "}"
//   expr    = mul ("+" mul | "-" mul)*
//   mul     = unary ("*" unary | "/" unary)*
//   unary   = ("+" | "-") unary | primary
//   primary = num | "(" expr ")"
//
// Precedence is the shape of the call chain: expr calls mul, so mul binds
// tighter. Left associativity is the loop, not the recursion - folding into
// lhs as it goes is what makes 8-3-2 parse as (8-3)-2.
#pragma once

#include "Ast.h"
#include "Lexer.h"

#include <vector>

class Source;

class Parser {
public:
    Parser(const Source &src, std::vector<Token> tokens)
        : src_(src), tokens_(std::move(tokens)) {}

    NodePtr parse();

private:
    const Source &src_;
    std::vector<Token> tokens_;
    std::size_t at_ = 0;

    const Token &peek() const { return tokens_[at_]; }
    bool consume(const char *s);
    void expect(const char *s);

    NodePtr expr();
    NodePtr mul();
    NodePtr unary();
    NodePtr primary();
};
