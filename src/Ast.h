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
class Postfix;
class StrLit;
class VaStart;
class VaArg;
class MemberAccess;
class ExprStmt;
class Return;
class Block;
class If;
class While;
class For;
class DoWhile;
class Switch;
class Case;
class Goto;
class Label;
class Conditional;
class Comma;
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
    virtual void visit(const Postfix &) = 0;
    virtual void visit(const Call &) = 0;
    virtual void visit(const Cast &) = 0;
    virtual void visit(const StrLit &) = 0;
    virtual void visit(const VaStart &) = 0;
    virtual void visit(const VaArg &) = 0;
    virtual void visit(const MemberAccess &) = 0;
    virtual void visit(const ExprStmt &) = 0;
    virtual void visit(const Return &) = 0;
    virtual void visit(const Block &) = 0;
    virtual void visit(const If &) = 0;
    virtual void visit(const While &) = 0;
    virtual void visit(const For &) = 0;
    virtual void visit(const DoWhile &) = 0;
    virtual void visit(const Switch &) = 0;
    virtual void visit(const Case &) = 0;
    virtual void visit(const Goto &) = 0;
    virtual void visit(const Label &) = 0;
    virtual void visit(const Conditional &) = 0;
    virtual void visit(const Comma &) = 0;
    virtual void visit(const Break &) = 0;
    virtual void visit(const Continue &) = 0;
};

class Node {
public:
    virtual ~Node() = default;
    virtual void accept(Visitor &v) const = 0;
};

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

enum class BinOp { Add, Sub, Mul, Div, Mod, Shl, Shr,
                   BitAnd, BitOr, BitXor,
                   Eq, Ne, Lt, Le, Gt, Ge, LAnd, LOr };

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

class Var final : public Expr {
public:
    static Var *local(std::string name, int offset) { return new Var(std::move(name), true, offset); }
    static Var *global(std::string name) { return new Var(std::move(name), false, 0); }

    const std::string &name() const { return name_; }
    bool isLocal() const { return isLocal_; }
    int offset() const { return offset_; }
    bool readOnly() const { return readOnly_; }
    void setReadOnly(bool r) { readOnly_ = r; }
    bool noAddress() const { return noAddress_; }
    void setNoAddress(bool n) { noAddress_ = n; }
    void accept(Visitor &v) const override { v.visit(*this); }

private:
    Var(std::string name, bool isLocal, int offset)
        : name_(std::move(name)), isLocal_(isLocal), offset_(offset) {}
    std::string name_;
    bool isLocal_;
    int offset_;
    bool readOnly_ = false;
    bool noAddress_ = false;
};

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

// __builtin_va_start(ap). The named-parameter count it needs is a property of
// the function being compiled, not of this expression, so unlike the va_start
// macro that wraps it this node does not carry the last named parameter.
class VaStart final : public Expr {
public:
    explicit VaStart(ExprPtr list) : list_(std::move(list)) {}
    const Expr &list() const { return *list_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr list_;
};

// __builtin_va_arg(ap, T). The type is the whole point of this node: under
// System V it decides which of the two register files the argument was passed
// in and therefore which offset to test and step, and under Microsoft x64 it
// decides only how to read a slot whose width is always eight. Its type comes
// from setType rather than from an operand, because there is no operand to
// derive it from - the type was written in the source and consumed by the
// parser.
class VaArg final : public Expr {
public:
    explicit VaArg(ExprPtr list) : list_(std::move(list)) {}
    const Expr &list() const { return *list_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr list_;
};

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

class Call final : public Expr {
public:
    Call(std::string name, ExprPtr callee, std::vector<ExprPtr> args, bool variadic,
         int resultSlot = 0, int namedArgs = -1, std::vector<int> argSlots = {})
        : name_(std::move(name)), callee_(std::move(callee)),
          args_(std::move(args)), variadic_(variadic), resultSlot_(resultSlot),
          namedArgs_(namedArgs < 0 ? static_cast<int>(args_.size()) : namedArgs),
          argSlots_(std::move(argSlots)) {}
    const std::string &name() const { return name_; }
    const std::vector<ExprPtr> &args() const { return args_; }
    const Expr *callee() const { return callee_.get(); }
    bool isVariadic() const { return variadic_; }
    int resultSlot() const { return resultSlot_; }
    // How many arguments the prototype named. Past this they are the variadic
    // part, which Apple's arm64 passes on the stack rather than in registers.
    int namedArgs() const { return namedArgs_; }
    // Frame space for a copy of each aggregate argument, one slot per argument
    // and 0 for the ones that need none. Both ABIs that pass a large aggregate
    // as a pointer require the *caller* to make the copy - the callee is
    // entitled to write through that pointer, so handing it the original would
    // let it modify the caller's object.
    int argSlot(std::size_t i) const {
        return i < argSlots_.size() ? argSlots_[i] : 0;
    }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    ExprPtr callee_;
    std::vector<ExprPtr> args_;
    bool variadic_;
    int resultSlot_;
    int namedArgs_;
    std::vector<int> argSlots_;
};

class Postfix final : public Expr {
public:
    Postfix(ExprPtr target, bool increment, long step)
        : target_(std::move(target)), increment_(increment), step_(step) {}
    const Expr &target() const { return *target_; }
    bool increment() const { return increment_; }
    long step() const { return step_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr target_;
    bool increment_;
    long step_;
};

class Cast final : public Expr {
public:
    Cast(const Type *to, ExprPtr value) : value_(std::move(value)) { setType(to); }
    const Expr &value() const { return *value_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr value_;
};

class Comma final : public Expr {
public:
    Comma(ExprPtr left, ExprPtr right)
        : left_(std::move(left)), right_(std::move(right)) {}
    const Expr &left() const { return *left_; }
    const Expr &right() const { return *right_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr left_, right_;
};

class Conditional final : public Expr {
public:
    Conditional(ExprPtr cond, ExprPtr thenArm, ExprPtr elseArm)
        : cond_(std::move(cond)), then_(std::move(thenArm)),
          else_(std::move(elseArm)) {}
    const Expr &cond() const { return *cond_; }
    const Expr &thenArm() const { return *then_; }
    const Expr &elseArm() const { return *else_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr cond_, then_, else_;
};

class MemberAccess final : public Expr {
public:
    MemberAccess(ExprPtr object, std::string name, int offset,
                 int width = 0, int bitOffset = 0)
        : object_(std::move(object)), name_(std::move(name)), offset_(offset),
          width_(width), bitOffset_(bitOffset) {}
    const Expr &object() const { return *object_; }
    const std::string &name() const { return name_; }
    int offset() const { return offset_; }
    int width() const { return width_; }
    int bitOffset() const { return bitOffset_; }
    bool isBitField() const { return width_ != 0; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr object_;
    std::string name_;
    int offset_;
    int width_;
    int bitOffset_;
};

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
    bool hasValue() const { return value_ != nullptr; }
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
    const Stmt *elseArm() const { return else_.get(); }
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

class For final : public Stmt {
public:
    For(StmtPtr init, ExprPtr cond, ExprPtr step, StmtPtr body)
        : init_(std::move(init)), cond_(std::move(cond)),
          step_(std::move(step)), body_(std::move(body)) {}
    const Stmt *init() const { return init_.get(); }
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

class Case final : public Stmt {
public:
    Case(long value, bool isDefault, int id, StmtPtr body)
        : value_(value), isDefault_(isDefault), id_(id), body_(std::move(body)) {}
    long value() const { return value_; }
    bool isDefault() const { return isDefault_; }
    int id() const { return id_; }
    const Stmt &body() const { return *body_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    long value_;
    bool isDefault_;
    int id_;
    StmtPtr body_;
};

class Switch final : public Stmt {
public:
    Switch(ExprPtr cond, StmtPtr body, std::vector<const Case *> cases,
           const Case *deflt)
        : cond_(std::move(cond)), body_(std::move(body)),
          cases_(std::move(cases)), default_(deflt) {}
    const Expr &cond() const { return *cond_; }
    const Stmt &body() const { return *body_; }
    const std::vector<const Case *> &cases() const { return cases_; }
    const Case *defaultCase() const { return default_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    ExprPtr cond_;
    StmtPtr body_;
    std::vector<const Case *> cases_;
    const Case *default_;
};

class Goto final : public Stmt {
public:
    explicit Goto(std::string label) : label_(std::move(label)) {}
    const std::string &label() const { return label_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string label_;
};

class Label final : public Stmt {
public:
    Label(std::string name, StmtPtr body)
        : name_(std::move(name)), body_(std::move(body)) {}
    const std::string &name() const { return name_; }
    const Stmt &body() const { return *body_; }
    void accept(Visitor &v) const override { v.visit(*this); }
private:
    std::string name_;
    StmtPtr body_;
};

class Break final : public Stmt {
public:
    void accept(Visitor &v) const override { v.visit(*this); }
};

class Continue final : public Stmt {
public:
    void accept(Visitor &v) const override { v.visit(*this); }
};

struct Param {
    const Type *type;
    int offset;
};

class Function {
public:
    Function(std::string name, const Type *returns, std::vector<Param> params,
             StmtPtr body, int frameSize, bool isStatic, int sretSlot = 0,
             bool variadic = false, int regSaveSlot = 0)
        : name_(std::move(name)), returns_(returns), params_(std::move(params)),
          body_(std::move(body)), frameSize_(frameSize), isStatic_(isStatic),
          sretSlot_(sretSlot), variadic_(variadic), regSaveSlot_(regSaveSlot) {}
    const std::string &name() const { return name_; }
    const Type *returns() const { return returns_; }
    const std::vector<Param> &params() const { return params_; }
    const Stmt &body() const { return *body_; }
    int frameSize() const { return frameSize_; }
    bool isStatic() const { return isStatic_; }
    int sretSlot() const { return sretSlot_; }
    bool isVariadic() const { return variadic_; }
    // Where the 176 bytes holding the incoming argument registers begin, as a
    // positive offset below %rbp. Zero when the function is not variadic.
    int regSaveSlot() const { return regSaveSlot_; }
private:
    std::string name_;
    const Type *returns_;
    std::vector<Param> params_;
    StmtPtr body_;
    int frameSize_;
    bool isStatic_;
    int sretSlot_;
    bool variadic_;
    int regSaveSlot_;
};

struct GlobalPiece {
    int offset;
    int size;
    long value;
    // When this is not empty the piece is an address constant: the address of
    // 'symbol', plus 'value' bytes. The linker resolves it - the program never
    // computes it, which is what makes it a constant at all. C90 6.5.7 allows
    // one wherever a file-scope initialiser is wanted, and a table of pointers
    // to other objects is what it is for.
    std::string symbol;
};

struct Global {
    std::string name;
    const Type *type;
    std::vector<GlobalPiece> init;
    bool hasInit;
    bool isStatic;
    // Carried here for the backend's sake rather than the parser's. The parser
    // already knows this - it is how an assignment to a const object is
    // refused - but the section an object lands in is decided where the object
    // is emitted, and until now nothing that far down could ask.
    bool isConst;
};

// A string literal, as the bytes it actually occupies. 'bytes' is the whole
// object including its terminator, rather than the characters with a
// terminator understood - which is what lets a wide literal live here beside a
// narrow one: L"hi" on a four-byte-wchar_t target is twelve bytes, three of
// them the terminator, and none of that is something an emitter should have to
// work out for itself.
struct StringLit {
    std::string label;
    std::string bytes;
    int width;          // 1 for a narrow literal, sizeof(wchar_t) for a wide one
};

struct Program {
    std::vector<Function> functions;
    std::vector<Global> globals;
    std::vector<StringLit> strings;
};
