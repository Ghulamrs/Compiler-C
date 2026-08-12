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
class ExprStmt;
class Return;
class Block;
class If;
class While;

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
    virtual void visit(const ExprStmt &) = 0;
    virtual void visit(const Return &) = 0;
    virtual void visit(const Block &) = 0;
    virtual void visit(const If &) = 0;
    virtual void visit(const While &) = 0;
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

enum class BinOp { Add, Sub, Mul, Div, Mod, Shl, Shr, Eq, Ne, Lt, Le, Gt, Ge };

// ---- expressions ----

class Num final : public Expr {
public:
    explicit Num(long v) : value_(v) {}
    long value() const { return value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    long value_;
};

// A local, addressed as a negative offset from %rbp. The offset is settled by
// the parser, which is the only stage that knows the declaration order.
class Var final : public Expr {
public:
    Var(std::string name, int offset) : name_(std::move(name)), offset_(offset) {}
    const std::string &name() const { return name_; }
    int offset() const { return offset_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    int offset_;
};

// Only a simple local is assignable today, so the target is an offset rather
// than a general lvalue expression. That changes when pointers arrive.
class Assign final : public Expr {
public:
    Assign(std::string name, int offset, ExprPtr value)
        : name_(std::move(name)), offset_(offset), value_(std::move(value)) {}
    const std::string &name() const { return name_; }
    int offset() const { return offset_; }
    const Expr &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    int offset_;
    ExprPtr value_;
};

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
    Call(std::string name, std::vector<ExprPtr> args)
        : name_(std::move(name)), args_(std::move(args)) {}
    const std::string &name() const { return name_; }
    const std::vector<ExprPtr> &args() const { return args_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    std::vector<ExprPtr> args_;
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
    Function(std::string name, std::vector<Param> params, StmtPtr body, int frameSize)
        : name_(std::move(name)), params_(std::move(params)),
          body_(std::move(body)), frameSize_(frameSize) {}
    const std::string &name() const { return name_; }
    const std::vector<Param> &params() const { return params_; }
    const Stmt &body() const { return *body_; }
    int frameSize() const { return frameSize_; }
private:
    std::string name_;
    std::vector<Param> params_;
    StmtPtr body_;
    int frameSize_;
};

using Program = std::vector<Function>;
