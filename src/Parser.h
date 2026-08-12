// Parser.h - tokens to a tree, by recursive descent.
//
// Hand-written rather than generated. bison 3.7.4 is on this box and would do
// it, but a C grammar's hard parts - the declarator syntax, and the typedef
// ambiguity where (A)*b is a cast or a multiplication depending on how A was
// declared - are exactly what a generator does not solve for you. Both need
// the parser to consult a symbol table as it goes, which is why locals live in
// this class and not in a later pass.
//
// The grammar accepted today:
//
//   program     = "int" "main" "(" ["void"] ")" block
//   block       = "{" (declaration | statement)* "}"
//   declaration = "int" ident ["=" expr] ";"
//   statement   = "return" expr ";"
//               | "if" "(" expr ")" statement ["else" statement]
//               | "while" "(" expr ")" statement
//               | block
//               | [expr] ";"
//   expr        = assign
//   assign      = equality ["=" assign]          right associative
//   equality    = relational (("==" | "!=") relational)*
//   relational  = add (("<" | "<=" | ">" | ">=") add)*
//   add         = mul (("+" | "-") mul)*
//   mul         = unary (("*" | "/" | "%") unary)*
//   unary       = ("+" | "-") unary | primary
//   primary     = num | ident | "(" expr ")"
//
// Precedence is the shape of the call chain: assign sits above equality, which
// sits above relational, and so on down to primary. Left associativity is the
// loop; assignment is the one that recurses instead, because a = b = c has to
// group to the right.
#pragma once

#include "Ast.h"
#include "Lexer.h"

#include <string>
#include <vector>

class Source;

class Parser {
public:
    Parser(const Source &src, std::vector<Token> tokens)
        : src_(src), tokens_(std::move(tokens)) {}

    Function parse();

private:
    struct Local {
        std::string name;
        int offset;
    };

    const Source &src_;
    std::vector<Token> tokens_;
    std::size_t at_ = 0;

    // One flat scope for the whole function. C gives every block its own, and
    // permits shadowing; that needs a stack of scopes and is a separate change.
    // A linear search over a vector rather than a map: functions have few
    // locals, and <unordered_map> in a shared header is measurable on this box.
    std::vector<Local> locals_;
    int frameSize_ = 0;

    const Token &peek() const { return tokens_[at_]; }
    bool consume(const char *s);
    void expect(const char *s);

    int declare(const std::string &name, std::size_t pos);
    int lookup(const std::string &name, std::size_t pos) const;

    StmtPtr block();
    StmtPtr statement();
    StmtPtr declaration();

    ExprPtr expr();
    ExprPtr assign();
    ExprPtr equality();
    ExprPtr relational();
    ExprPtr add();
    ExprPtr mul();
    ExprPtr unary();
    ExprPtr primary();
};
