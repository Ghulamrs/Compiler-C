// Parser.h - tokens to a typed tree, by recursive descent.
//
// This stage does the type checking as well as the parsing, because C cannot
// separate them: the declarator grammar and the typedef ambiguity both need a
// symbol table consulted while parsing. See docs/TYPES.md.
//
// Every conversion the language performs is made explicit here, as a Cast node
// in the tree. Code generation knows no conversion rule; it only widens and
// narrows what it is handed.
//
// The grammar accepted today:
//
//   program     = (global | prototype | function)*
//   global      = specifiers declarator ["=" number] ";"
//   function    = specifiers declarator "(" params ")" (block | ";")
//   declarator  = "*"* ident ("[" number "]")*
//   specifiers  = ["static"|"extern"] ("void"|"char"|"short"|"int"|"long"
//                                     |"signed"|"unsigned")+
//   block       = "{" (declaration | statement)* "}"
//   declaration = specifiers declarator ["=" expr] ";"
//   statement   = "return" expr ";" | "if" ... | "while" ... | block | [expr] ";"
//   assign      = logicalOr ["=" assign]
//   logicalOr   = logicalAnd ("||" logicalAnd)*
//   logicalAnd  = equality ("&&" equality)*
//   equality    = relational (("==" | "!=") relational)*
//   relational  = shift (("<" | "<=" | ">" | ">=") shift)*
//   shift       = add (("<<" | ">>") add)*
//   add         = mul (("+" | "-") mul)*
//   mul         = cast (("*" | "/" | "%") cast)*
//   cast        = "(" typename ")" cast | unary
//   unary       = ("+"|"-"|"!"|"&"|"*") cast | "sizeof" unary
//               | "sizeof" "(" typename ")" | postfix
//   postfix     = primary ("[" expr "]")*
//   primary     = num | string | ident | ident "(" args ")" | "(" expr ")"
//
// Not accepted: parenthesised declarators, so "int (*p)[10]" - a pointer to an
// array - cannot be written. "int *p[10]", an array of pointers, can. The
// suffix binding tighter than the prefix is why, and the parentheses that undo
// it need a recursive declarator parser.
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
           TypeTable &types, const Target &target)
        : src_(src), tokens_(std::move(tokens)), types_(types), target_(target) {}

    Program parse();

private:
    struct Local {
        std::string name;
        int offset;
        const Type *type;
    };

    struct GlobalSym {
        std::string name;
        const Type *type;
    };

    struct Signature {
        std::string name;
        const Type *returns;
        std::vector<const Type *> params;
        bool variadic;
        bool defined;
        std::size_t pos;
    };

    // What a declarator produced.
    struct Declared {
        std::string name;
        const Type *type;
        std::size_t pos;
    };

    enum StorageClass { StorageNone, StorageStatic, StorageExtern };

    const Source &src_;
    std::vector<Token> tokens_;
    TypeTable &types_;
    const Target &target_;

    std::size_t at_ = 0;
    std::vector<Local> locals_;
    int frameSize_ = 0;
    const Type *returnType_ = nullptr;

    std::vector<Signature> functions_;
    std::vector<GlobalSym> globals_;
    int strings_ = 0;

    const Token &peek() const { return tokens_[at_]; }
    const Token &peekAt(std::size_t n) const;
    bool consume(const char *s);
    void expect(const char *s);
    std::string expectIdent(const char *what);
    long expectNumber(const char *what);

    // ---- types ----
    bool atTypeName() const;
    bool atDeclarationStart() const;
    const Type *specifiers(StorageClass *storage);
    Declared declarator(const Type *base);
    const Type *promote(const Type *t) const;
    const Type *usualArithmetic(const Type *a, const Type *b) const;
    ExprPtr convert(ExprPtr e, const Type *to) const;
    const Type *unsignedVersion(const Type *t) const;

    // An array used as a value is the address of its first element. Everywhere
    // except sizeof and '&', which is why this is applied at the use and not
    // when the array is built.
    ExprPtr decay(ExprPtr e);
    void requireScalar(const Expr &e, std::size_t pos, const char *what);

    // ---- symbols ----
    int declare(const std::string &name, const Type *type, std::size_t pos);
    const Local *findLocal(const std::string &name) const;
    const GlobalSym *findGlobal(const std::string &name) const;
    void declareFunction(const std::string &name, const Type *returns,
                         const std::vector<const Type *> &params,
                         bool variadic, bool defining, std::size_t pos);
    // char and short become int, float becomes double, past the last named
    // parameter of a variadic call. printf("%f", 1.5f) works because of this.
    ExprPtr defaultPromote(ExprPtr e);
    const Signature &lookupFunction(const std::string &name, std::size_t pos) const;

    // ---- grammar ----
    void topLevel(Program &program);
    StmtPtr block();
    StmtPtr statement();
    StmtPtr declaration();

    ExprPtr expr();
    ExprPtr assign();
    ExprPtr logicalOr();
    ExprPtr logicalAnd();
    ExprPtr equality();
    ExprPtr relational();
    ExprPtr shift();
    ExprPtr add();
    ExprPtr mul();
    ExprPtr castExpr();
    ExprPtr unary();
    ExprPtr postfix();
    ExprPtr primary(Program *program);

    ExprPtr arithmetic(BinOp op, ExprPtr lhs, ExprPtr rhs, std::size_t pos);
    ExprPtr comparison(BinOp op, ExprPtr lhs, ExprPtr rhs);
    ExprPtr pointerAdd(ExprPtr p, ExprPtr n);
    ExprPtr pointerSub(ExprPtr l, ExprPtr r, std::size_t pos);

    // Set while parsing a function body, so a string literal can be recorded.
    Program *current_ = nullptr;
};
