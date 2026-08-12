// Ast.h - the tree, and the visitor that walks it.
//
// Expressions and statements are separate hierarchies. It costs a second base
// class and buys a guarantee the compiler enforces: an If cannot be handed a
// statement as its condition, and a Block cannot hold a bare expression that
// nothing consumes.
//
// Code generation is a visitor rather than a virtual gen() on each node. With
// one backend the two look alike; with three - x86-64 SysV, x86-64 Windows,
// arm64 Apple - a gen() per node means every node knowing every target.
#pragma once

#include "Type.h"

#include <memory>
#include <string>
#include <vector>

class Num;
class Var;
class Assign;
class Unary;
class Binary;
class Call;
class Cast;
class StrLit;
class MemberAccess;
class ExprStmt;
class Return;
class Block;
class If;
class While;
class For;
class DoWhile;
class Break;
class Continue;

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(const Num &) = 0;
    virtual void visit(const Var &) = 0;
    virtual void visit(const Assign &) = 0;
    virtual void visit(const Unary &) = 0;
    virtual void visit(const Binary &) = 0;
    virtual void visit(const Call &) = 0;
    virtual void visit(const Cast &) = 0;
    virtual void visit(const StrLit &) = 0;
    virtual void visit(const MemberAccess &) = 0;
    virtual void visit(const ExprStmt &) = 0;
    virtual void visit(const Return &) = 0;
    virtual void visit(const Block &) = 0;
    virtual void visit(const If &) = 0;
    virtual void visit(const While &) = 0;
    virtual void visit(const For &) = 0;
    virtual void visit(const DoWhile &) = 0;
    virtual void visit(const Break &) = 0;
    virtual void visit(const Continue &) = 0;
};

class Node {
public:
    virtual ~Node() = default;
    virtual void accept(Visitor &v) const = 0;
};

// Every expression carries its type. The parser fills it in - it is the only
// stage that can, since working it out needs the symbol tables. Code generation
// then reads it to choose an instruction width and a signed or unsigned form.
class Expr : public Node {
public:
    const Type *type() const { return type_; }
    void setType(const Type *t) { type_ = t; }
private:
    const Type *type_ = nullptr;
};

class Stmt : public Node {};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// LAnd and LOr sit in this list but are not ordinary binary operators: they
// evaluate their right side conditionally, so code generation branches around
// it rather than computing both and combining. They are here rather than in
// their own node because everything else about them - two operands, one result
// - is the same shape.
enum class BinOp { Add, Sub, Mul, Div, Mod, Shl, Shr,
                   BitAnd, BitOr, BitXor,
                   Eq, Ne, Lt, Le, Gt, Ge, LAnd, LOr };

// ---- expressions ----

// One node for both, chosen by the expression's type rather than by a second
// class: a constant is a constant, and everything that reads it already has to
// consult the type to know which register it belongs in.
class Num final : public Expr {
public:
    explicit Num(long v) : value_(v) {}
    explicit Num(double d) : dvalue_(d) {}
    long value() const { return value_; }
    double dvalue() const { return dvalue_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    long value_ = 0;
    double dvalue_ = 0;
};

// A named object. A local is a negative offset from %rbp; a global is a symbol
// the linker resolves, reached through %rip. Both are lvalues, which is what
// matters to everything above.
class Var final : public Expr {
public:
    static Var *local(std::string name, int offset) { return new Var(std::move(name), true, offset); }
    static Var *global(std::string name) { return new Var(std::move(name), false, 0); }

    const std::string &name() const { return name_; }
    bool isLocal() const { return isLocal_; }
    int offset() const { return offset_; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    Var(std::string name, bool isLocal, int offset)
        : name_(std::move(name)), isLocal_(isLocal), offset_(offset) {}
    std::string name_;
    bool isLocal_;
    int offset_;
};

// A string literal, emitted once into .rodata and referred to by its label.
// Its type is char[N+1] - the terminating zero is part of it - so sizeof "abc"
// is 4, and it decays to char* like any other array.
class StrLit final : public Expr {
public:
    StrLit(std::string label, std::string text)
        : label_(std::move(label)), text_(std::move(text)) {}
    const std::string &label() const { return label_; }
    const std::string &text() const { return text_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string label_;
    std::string text_;
};

// The target is now an expression, not a name. Pointers made that necessary:
// "*p = x" and "a[i] = x" assign to places no name describes. Code generation
// asks the target for its address rather than being handed an offset, and the
// parser has already refused anything that is not an lvalue.
class Assign final : public Expr {
public:
    Assign(ExprPtr target, ExprPtr value)
        : target_(std::move(target)), value_(std::move(value)) {}
    const Expr &target() const { return *target_; }
    const Expr &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr target_, value_;
};

// op is one of '-', '!', '&' (address of) or '*' (dereference). The last two
// are not arithmetic at all: '&' produces an address without reading anything,
// and '*' names a place rather than a value, so both are handled by the
// address path in code generation rather than the value path.
class Unary final : public Expr {
public:
    Unary(char op, ExprPtr operand) : op_(op), operand_(std::move(operand)) {}
    char op() const { return op_; }
    const Expr &operand() const { return *operand_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    char op_;
    ExprPtr operand_;
};

class Binary final : public Expr {
public:
    Binary(BinOp op, ExprPtr lhs, ExprPtr rhs)
        : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
    BinOp op() const { return op_; }
    const Expr &lhs() const { return *lhs_; }
    const Expr &rhs() const { return *rhs_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    BinOp op_;
    ExprPtr lhs_, rhs_;
};

// A call. By the time one of these exists the parser has already found the
// name in the function table and checked the argument count against the
// prototype, so code generation can emit the call without further questions.
// C89 would have allowed an undeclared name here and assumed it returned int;
// this compiler refuses, because a misspelled name that links against nothing
// is a worse error message than one the parser gives with a line number.
class Call final : public Expr {
public:
    Call(std::string name, std::vector<ExprPtr> args, bool variadic)
        : name_(std::move(name)), args_(std::move(args)), variadic_(variadic) {}
    const std::string &name() const { return name_; }
    const std::vector<ExprPtr> &args() const { return args_; }
    // A variadic callee reads %al for the number of vector registers used.
    // Setting it wrongly is how printf("%f") reads the wrong argument.
    bool isVariadic() const { return variadic_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    std::vector<ExprPtr> args_;
    bool variadic_;
};

// An explicit conversion, and also every implicit one. The parser inserts these
// wherever the language says a conversion happens - the integer promotions, the
// usual arithmetic conversions, assignment, and prototyped arguments - so that
// code generation never has to know a conversion rule. It only has to know how
// to widen or narrow what it is handed.
class Cast final : public Expr {
public:
    Cast(const Type *to, ExprPtr value) : value_(std::move(value)) { setType(to); }
    const Expr &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr value_;
};

// s.m and p->m. The parser lowers p->m to (*p).m, so only one node is needed:
// the difference between them is which expression is on the left, not what
// happens afterwards. An lvalue, because a member of a place is a place.
class MemberAccess final : public Expr {
public:
    MemberAccess(ExprPtr object, std::string name, int offset)
        : object_(std::move(object)), name_(std::move(name)), offset_(offset) {}
    const Expr &object() const { return *object_; }
    const std::string &name() const { return name_; }
    int offset() const { return offset_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr object_;
    std::string name_;
    int offset_;
};

// ---- statements ----

class ExprStmt final : public Stmt {
public:
    explicit ExprStmt(ExprPtr e) : expr_(std::move(e)) {}
    const Expr &expr() const { return *expr_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr expr_;
};

class Return final : public Stmt {
public:
    explicit Return(ExprPtr value) : value_(std::move(value)) {}
    const Expr &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr value_;
};

class Block final : public Stmt {
public:
    explicit Block(std::vector<StmtPtr> body) : body_(std::move(body)) {}
    const std::vector<StmtPtr> &body() const { return body_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::vector<StmtPtr> body_;
};

class If final : public Stmt {
public:
    If(ExprPtr cond, StmtPtr thenArm, StmtPtr elseArm)
        : cond_(std::move(cond)), then_(std::move(thenArm)), else_(std::move(elseArm)) {}
    const Expr &cond() const { return *cond_; }
    const Stmt &thenArm() const { return *then_; }
    const Stmt *elseArm() const { return else_.get(); }   // null when absent
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr cond_;
    StmtPtr then_, else_;
};

class While final : public Stmt {
public:
    While(ExprPtr cond, StmtPtr body)
        : cond_(std::move(cond)), body_(std::move(body)) {}
    const Expr &cond() const { return *cond_; }
    const Stmt &body() const { return *body_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr cond_;
    StmtPtr body_;
};

// for (init; cond; step) body.
//
// Kept as its own node rather than lowered to a while, because "continue" must
// jump to the step and not to the condition. Lowering would put the step inside
// the body, where a continue would skip it - which is the one thing about for
// that a while cannot express.
class For final : public Stmt {
public:
    For(StmtPtr init, ExprPtr cond, ExprPtr step, StmtPtr body)
        : init_(std::move(init)), cond_(std::move(cond)),
          step_(std::move(step)), body_(std::move(body)) {}
    const Stmt *init() const { return init_.get(); }   // all three are optional
    const Expr *cond() const { return cond_.get(); }
    const Expr *step() const { return step_.get(); }
    const Stmt &body() const { return *body_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    StmtPtr init_;
    ExprPtr cond_, step_;
    StmtPtr body_;
};

class DoWhile final : public Stmt {
public:
    DoWhile(StmtPtr body, ExprPtr cond)
        : body_(std::move(body)), cond_(std::move(cond)) {}
    const Stmt &body() const { return *body_; }
    const Expr &cond() const { return *cond_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    StmtPtr body_;
    ExprPtr cond_;
};

class Break final : public Stmt {
public:
    void accept(Visitor &v) const override { v.visit(*this); }
};

class Continue final : public Stmt {
public:
    void accept(Visitor &v) const override { v.visit(*this); }
};

// ---- what a translation unit is ----

// Parameters keep their type and their slot, because the prologue has to store
// each argument register with the width of its own type - a char parameter is
// one byte in the frame, not eight.
struct Param {
    const Type *type;
    int offset;
};

class Function {
public:
    Function(std::string name, const Type *returns, std::vector<Param> params,
             StmtPtr body, int frameSize)
        : name_(std::move(name)), returns_(returns), params_(std::move(params)),
          body_(std::move(body)), frameSize_(frameSize) {}
    const std::string &name() const { return name_; }
    const Type *returns() const { return returns_; }
    const std::vector<Param> &params() const { return params_; }
    const Stmt &body() const { return *body_; }
    int frameSize() const { return frameSize_; }
private:
    std::string name_;
    const Type *returns_;
    std::vector<Param> params_;
    StmtPtr body_;
    int frameSize_;
};

// A file-scope object. Zero-initialised unless an initialiser was given, which
// today may only be an integer constant - a general constant expression
// evaluator is a separate piece of work.
struct Global {
    std::string name;
    const Type *type;
    long init;
    bool hasInit;
    bool isStatic;      // internal linkage: no .globl
};

struct Program {
    std::vector<Function> functions;
    std::vector<Global> globals;
    std::vector<std::pair<std::string, std::string>> strings;  // label, text
};
