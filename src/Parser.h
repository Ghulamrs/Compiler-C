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
//   statement   = "return" expr ";" | "if" ... | "while" ... | "for" ...
//               | "do" ... | "switch" "(" expr ")" statement
//               | "case" constant ":" statement | "default" ":" statement
//               | "break" ";" | "continue" ";" | block | [expr] ";"
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
#include <unordered_map>
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
        bool isConst = false;
        // Empty for an ordinary local. A static local lives in the data section
        // under this name instead of in the frame, which is the whole of what
        // "static" means here: the storage outlives the call.
        std::string staticName;
    };

    struct GlobalSym {
        std::string name;
        const Type *type;
        bool isConst = false;
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

    enum StorageClass { StorageNone, StorageStatic, StorageExtern, StorageTypedef };

    // const and volatile, which qualify an object rather than name a type.
    //
    // They are deliberately not part of Type. Making "const char *" a distinct
    // interned type from "char *" would reach every comparison in this parser -
    // assignment, calls and '?:' all decide compatibility by pointer equality on
    // interned types - and the checking it would buy is checking through
    // pointers. What is here instead catches the direct case: an object
    // declared const cannot be assigned to. See docs/STATUS.md for what that
    // leaves uncaught, which is written down rather than left to be discovered.
    struct Qualifiers {
        bool isConst = false;
        bool isVolatile = false;
    };

    // A name introduced by typedef. The reason this table exists at all is the
    // ambiguity C cannot resolve without it: "(A)*b" is a cast if A is a
    // typedef name and a multiplication if it is a variable, and the grammar
    // alone cannot tell. Every place that asks "does a type start here?" asks
    // this too.
    struct TypedefName {
        std::string name;
        const Type *type;
    };

    // An enumerator is an int constant, and enum itself is an alias for int.
    struct EnumConst {
        std::string name;
        long value;
    };

    const Source &src_;
    std::vector<Token> tokens_;
    TypeTable &types_;
    const Target &target_;

    std::size_t at_ = 0;

    // Locals, innermost last, with scopeStarts_ marking where each block's
    // names begin; leaving a block truncates the list back to its mark.
    //
    // This was one flat scope per function until "for (int i = ...)" appeared
    // twice in the same function and the second was refused. That is ordinary
    // C, and a for that can only be used once is not a for.
    //
    // A name is looked up innermost outwards, so an inner declaration shadows
    // an outer one and a duplicate is only a duplicate within its own block.
    // Frame slots are not reused when a scope ends: the frame is a little
    // larger than it needs to be, and nothing is wrong.
    std::vector<Local> locals_;
    std::vector<std::size_t> scopeStarts_;
    int frameSize_ = 0;
    const Type *returnType_ = nullptr;
    // The function being parsed, for naming its static locals.
    std::string functionName_;
    // The data-section symbols its static locals have already taken. Two blocks
    // of one function may each declare a "static int n", and C makes those two
    // distinct objects - so the function's name and the variable's are not
    // enough to tell them apart.
    std::vector<std::string> staticSymbols_;

    // The vectors keep declaration order; the maps make finding a name cost
    // the same whether a file has ten of them or ten thousand.
    //
    // These were linear scans, and the cost was not theoretical: parsing was
    // quadratic in the number of functions, because every call and every
    // declaration walked the whole table. 8000 functions took 200 ms to parse
    // against 5.65 ms for 1000 - four times the functions for thirty-five
    // times the work. Lexing and code generation over the same files were
    // linear, which is what made the parser the obvious suspect.
    //
    // Locals stay a linear scan on purpose: the list is per function and short,
    // and a map per function would cost more to build than it saved.
    std::vector<Signature> functions_;
    std::unordered_map<std::string, std::size_t> functionIndex_;
    std::vector<GlobalSym> globals_;
    std::unordered_map<std::string, std::size_t> globalIndex_;
    std::vector<TypedefName> typedefs_;
    std::unordered_map<std::string, std::size_t> typedefIndex_;
    std::vector<EnumConst> enums_;
    std::unordered_map<std::string, std::size_t> enumIndex_;
    int strings_ = 0;
    int loopDepth_ = 0;     // continue needs one to be inside; break takes either
    int switchDepth_ = 0;
    int caseIds_ = 0;       // one per case label, unique across the unit

    // What the case labels inside the switch currently being parsed collect
    // into. A stack, because a switch may contain another, and each case
    // belongs to the innermost one.
    //
    // The Case pointers are not owned. Each is owned by the statement list it
    // was parsed into, which outlives the Switch node built from this.
    struct SwitchCtx {
        std::vector<const Case *> cases;
        const Case *deflt;
        // The promoted type of the controlling expression. Every case value is
        // converted to it, because that is what C compares them in.
        const Type *governing;
    };
    std::vector<SwitchCtx> switches_;

    // Labels have function scope, not block scope, so both of these are per
    // function and both are cleared when one begins. A goto may name a label
    // that has not been seen yet - a forward jump is the ordinary case, not the
    // exotic one - so the names cannot be resolved as they are parsed. They are
    // collected and checked against each other once the body is closed, which
    // is the earliest moment the answer is knowable.
    struct LabelDef { std::string name; std::size_t pos; };
    std::vector<LabelDef> labels_;
    std::vector<LabelDef> gotos_;

    const Token &peek() const { return tokens_[at_]; }
    const Token &peekAt(std::size_t n) const;
    bool consume(const char *s);
    void expect(const char *s);
    std::string expectIdent(const char *what);
    long expectNumber(const char *what);

    // ---- types ----
    bool atTypeName() const;
    const Type *findTypedef(const std::string &name) const;
    const EnumConst *findEnum(const std::string &name) const;
    const Type *structOrUnionSpecifier(Kind kind);
    const Type *enumSpecifier();
    bool atDeclarationStart() const;
    const Type *specifiers(StorageClass *storage, Qualifiers *quals = nullptr);
    // nameOptional is for a prototype's parameters, where C lets the name be
    // left out: "int printf(char *, ...)" names only types, and headers are
    // written that way. Everywhere else a declarator must name something.
    Declared declarator(const Type *base, bool nameOptional = false);
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
    // Space in the frame, with nothing put in the symbol table.
    int allocateFrameSlot(const Type *type);
    // A local with static storage duration. Takes no frame slot; the name it is
    // given in the data section is the function's own name and its own, joined,
    // so that two functions may each have a "static int n".
    void declareStaticLocal(const std::string &name, const Type *type,
                            std::size_t pos, const std::string &symbol);
    // Everything that writes through an lvalue asks this first: '=', the
    // compound assignments, and prefix ++ and --. One place, so a const object
    // cannot be assigned to by a route that forgot to check.
    void requireAssignable(const Expr &e, std::size_t pos, const char *what);
    const Local *findLocal(const std::string &name) const;
    void enterScope();
    void leaveScope();
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
    StmtPtr forStatement();
    StmtPtr switchStatement();
    StmtPtr caseLabel();
    StmtPtr gotoLabel();
    StmtPtr declaration();
    // Every goto in the function now closed has to name one of its labels.
    // Checked here rather than at the goto, because at the goto the answer is
    // not yet known.
    void resolveGotos();

    // An integer constant expression: a case value, an enumerator, a global's
    // initialiser, an array length. Parsed as an ordinary expression - which
    // type checks it and inserts every conversion for free - and then folded.
    long constantExpression(const char *what);
    // Folds e to an integer, or answers false. pos is where to report from:
    // expression nodes carry no position, so a constant reports at its first
    // token, which is the one a reader would look at anyway.
    bool fold(const Expr &e, long *out, std::size_t pos) const;
    // A constant as the type it is being compared in actually represents it.
    // "case 0x100000000" against an int switch is a label that can never be
    // taken, and it has to be recorded as the value it becomes rather than the
    // value written, or two distinct-looking labels can collide unnoticed.
    long narrowTo(long v, const Type *t) const;

    ExprPtr expr();
    ExprPtr assign();
    // cond ? a : b. Sits between assignment and ||, which is what makes
    // "a ? b : c = d" parse as "(a ? b : c) = d" and "a = b ? c : d" work.
    ExprPtr conditional();
    ExprPtr bitOr();
    ExprPtr bitXor();
    ExprPtr bitAnd();
    // x += e is x = x + e, and ++x is x += 1. Built here rather than given
    // nodes of their own, so there is one path to a store. The lvalue is
    // evaluated once in the tree, which is enough while no lvalue has a side
    // effect - a[i++] would need a temporary, and is refused until it can have
    // one.
    ExprPtr compound(BinOp op, ExprPtr target, ExprPtr value, std::size_t pos);
    ExprPtr incDec(ExprPtr target, bool increment, bool prefix, std::size_t pos);
    // A second copy of an lvalue, so "x += e" can read x and write x. Only
    // shapes whose evaluation is free are clonable; anything else is refused
    // rather than evaluated twice.
    ExprPtr cloneLvalue(const Expr &e, std::size_t pos);
    ExprPtr shiftOf(BinOp op, ExprPtr lhs, ExprPtr rhs);
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
