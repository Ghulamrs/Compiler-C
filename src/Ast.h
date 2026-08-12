// Ast.h - the tree, and the visitor that walks it.
//
// Code generation is a visitor rather than a virtual gen() on each node. With
// one backend the two look alike; with three - x86-64 SysV, x86-64 Windows,
// arm64 Apple - a gen() per node means every node knows every target, and
// adding the fourth means editing all of them. A visitor keeps each backend in
// its own file and lets a target be added without touching the tree.
#pragma once

#include <memory>

class Num;
class Unary;
class Binary;
class Return;

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(const Num &) = 0;
    virtual void visit(const Unary &) = 0;
    virtual void visit(const Binary &) = 0;
    virtual void visit(const Return &) = 0;
};

class Node {
public:
    virtual ~Node() = default;
    virtual void accept(Visitor &v) const = 0;
};

using NodePtr = std::unique_ptr<Node>;

class Num final : public Node {
public:
    explicit Num(long v) : value_(v) {}
    long value() const { return value_; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    long value_;
};

class Unary final : public Node {
public:
    Unary(char op, NodePtr operand) : op_(op), operand_(std::move(operand)) {}
    char op() const { return op_; }
    const Node &operand() const { return *operand_; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    char op_;
    NodePtr operand_;
};

class Binary final : public Node {
public:
    Binary(char op, NodePtr lhs, NodePtr rhs)
        : op_(op), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}
    char op() const { return op_; }
    const Node &lhs() const { return *lhs_; }
    const Node &rhs() const { return *rhs_; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    char op_;
    NodePtr lhs_, rhs_;
};

class Return final : public Node {
public:
    explicit Return(NodePtr value) : value_(std::move(value)) {}
    const Node &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    NodePtr value_;
};
