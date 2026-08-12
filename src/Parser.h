// Parser.h - tokens to a tree, by recursive descent, with the checking that
// makes a call trustworthy.
//
// Hand-written rather than generated. bison 3.7.4 is on this box and would do
// it, but a C grammar's hard parts - the declarator syntax, and the typedef
// ambiguity where (A)*b is a cast or a multiplication depending on how A was
// declared - are exactly what a generator does not solve for you. Both need
// the parser to consult a symbol table as it goes, which is why both tables
// below live here rather than in a later pass.
//
// Declaration before use is enforced. C89 permits an undeclared function to be
// called and assumes it returns int; this compiler refuses. A name that was
// never declared is a mistake far more often than it is an intention, and the
// parser can say so with a line number where the linker can only say that
// something, somewhere, is undefined. So putchar() needs its prototype at the
// top of the file, exactly as a header would have provided it.
//
// The grammar accepted today:
//
//   program     = (prototype | function)+
//   prototype   = "int" ident "(" params ")" ";"
//   function    = "int" ident "(" params ")" block
//   params      = "void" | empty | "int" ident ("," "int" ident)*
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
//   primary     = num | ident | ident "(" [expr ("," expr)*] ")" | "(" expr ")"
//
// Precedence is the shape of the call chain. Left associativity is the loop;
// assignment recurses instead, because a = b = c groups to the right.
//
// Every type in the language is int today, so checking a call against its
// prototype comes down to checking how many arguments it was given. The table
// is where the rest of that checking goes when there is more than one type.
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

    Program parse();

private:
    struct Local {
        std::string name;
        int offset;
    };

    // What is known about a function by the time it is called. 'defined' marks
    // the ones with a body here; the rest are expected from libc and are the
    // linker's problem, which is the one thing that genuinely cannot be checked
    // at this stage.
    struct Signature {
        std::string name;
        int params;
        bool defined;
        std::size_t pos;
    };

    const Source &src_;
    std::vector<Token> tokens_;
    std::size_t at_ = 0;

    // Reset for each function. One flat scope within it: C gives every block
    // its own and permits shadowing, which needs a stack of scopes and is a
    // separate change. A vector with a linear search rather than a map -
    // functions have few locals, and <unordered_map> in a shared header is
    // measurable on this box.
    std::vector<Local> locals_;
    int frameSize_ = 0;

    // Outlives every function, because that is the point of a prototype.
    std::vector<Signature> functions_;

    const Token &peek() const { return tokens_[at_]; }
    const Token &peekAt(std::size_t n) const;
    bool consume(const char *s);
    void expect(const char *s);
    std::string expectIdent(const char *what);

    int declare(const std::string &name, std::size_t pos);
    int lookup(const std::string &name, std::size_t pos) const;

    void declareFunction(const std::string &name, int params, bool defining,
                         std::size_t pos);
    const Signature &lookupFunction(const std::string &name, std::size_t pos) const;

    int parameterList();
    void functionOrPrototype(Program &program);
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
