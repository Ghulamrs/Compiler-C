// Parser.h - tokens to a typed tree, by recursive descent.
//
// This stage does the type checking as well as the parsing, because C cannot
// separate them: the declarator grammar and the typedef ambiguity both need a
// symbol table consulted while parsing. See docs/TYPES.md.
//
// Every conversion the language performs is made explicit here, as a Cast node
// in the tree - the integer promotions, the usual arithmetic conversions,
// assignment, and prototyped arguments. Code generation then never has to know
// a conversion rule; it only has to know how to widen or narrow.
//
// The grammar accepted today:
//
//   program     = (prototype | function)+
//   prototype   = specifiers ident "(" params ")" ";"
//   function    = specifiers ident "(" params ")" block
//   params      = "void" | empty | specifiers ident ("," specifiers ident)*
//   specifiers  = ("void"|"char"|"short"|"int"|"long"|"signed"|"unsigned")+
//   block       = "{" (declaration | statement)* "}"
//   declaration = specifiers ident ["=" expr] ";"
//   statement   = "return" expr ";" | "if" ... | "while" ... | block | [expr] ";"
//   expr        = assign
//   assign      = equality ["=" assign]
//   equality    = relational (("==" | "!=") relational)*
//   relational  = shift (("<" | "<=" | ">" | ">=") shift)*
//   shift       = add (("<<" | ">>") add)*
//   add         = mul (("+" | "-") mul)*
//   mul         = cast (("*" | "/" | "%") cast)*
//   cast        = "(" typename ")" cast | unary
//   unary       = ("+" | "-") cast | "sizeof" unary | "sizeof" "(" typename ")"
//               | primary
//   primary     = num | ident | ident "(" args ")" | "(" expr ")"
#pragma once

#include "Ast.h"
#include "Lexer.h"
#include "Type.h"

#include <string>
#include <vector>

class Source;

class Parser {
public:
    Parser(const Source &src, std::vector<Token> tokens,
           const TypeTable &types, const Target &target)
        : src_(src), tokens_(std::move(tokens)), types_(types), target_(target) {}

    Program parse();

private:
    struct Local {
        std::string name;
        int offset;
        const Type *type;
    };

    struct Signature {
        std::string name;
        const Type *returns;
        std::vector<const Type *> params;
        bool defined;
        std::size_t pos;
    };

    const Source &src_;
    std::vector<Token> tokens_;
    const TypeTable &types_;
    const Target &target_;

    std::size_t at_ = 0;
    std::vector<Local> locals_;
    int frameSize_ = 0;
    const Type *returnType_ = nullptr;   // of the function being parsed

    std::vector<Signature> functions_;

    const Token &peek() const { return tokens_[at_]; }
    const Token &peekAt(std::size_t n) const;
    bool consume(const char *s);
    void expect(const char *s);
    std::string expectIdent(const char *what);

    // ---- types ----
    bool atTypeName() const;
    const Type *specifiers();
    const Type *promote(const Type *t) const;
    const Type *usualArithmetic(const Type *a, const Type *b) const;
    ExprPtr convert(ExprPtr e, const Type *to) const;
    const Type *unsignedVersion(const Type *t) const;

    // ---- symbols ----
    int declare(const std::string &name, const Type *type, std::size_t pos);
    const Local &lookup(const std::string &name, std::size_t pos) const;
    void declareFunction(const std::string &name, const Type *returns,
                         const std::vector<const Type *> &params,
                         bool defining, std::size_t pos);
    const Signature &lookupFunction(const std::string &name, std::size_t pos) const;

    // ---- grammar ----
    void functionOrPrototype(Program &program);
    StmtPtr block();
    StmtPtr statement();
    StmtPtr declaration();

    ExprPtr expr();
    ExprPtr assign();
    ExprPtr equality();
    ExprPtr relational();
    ExprPtr shift();
    ExprPtr add();
    ExprPtr mul();
    ExprPtr castExpr();
    ExprPtr unary();
    ExprPtr primary();

    ExprPtr arithmetic(BinOp op, ExprPtr lhs, ExprPtr rhs);
    ExprPtr comparison(BinOp op, ExprPtr lhs, ExprPtr rhs);
};
