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
        std::string staticName;
    };

    struct GlobalSym {
        std::string name;
        const Type *type;
        bool isConst = false;
        bool emitted = false;
        bool hasInit = false;
    };

    struct Signature {
        std::string name;
        const Type *returns;
        std::vector<const Type *> params;
        bool variadic;
        bool defined;
        std::size_t pos;
    };

    struct Declared {
        std::string name;
        const Type *type;
        std::size_t pos;
    };

    enum StorageClass { StorageNone, StorageStatic, StorageExtern, StorageTypedef };

    struct Qualifiers {
        bool isConst = false;
        bool isVolatile = false;
    };

    struct TypedefName {
        std::string name;
        const Type *type;
    };

    struct EnumConst {
        std::string name;
        long value;
    };

    const Source &src_;
    std::vector<Token> tokens_;
    TypeTable &types_;
    const Target &target_;

    std::size_t at_ = 0;

    std::vector<Local> locals_;
    std::vector<std::size_t> scopeStarts_;
    int frameSize_ = 0;
    const Type *returnType_ = nullptr;
    std::string functionName_;
    std::vector<std::string> staticSymbols_;

    std::vector<Signature> functions_;
    std::unordered_map<std::string, std::size_t> functionIndex_;
    std::vector<GlobalSym> globals_;
    std::unordered_map<std::string, std::size_t> globalIndex_;
    std::vector<TypedefName> typedefs_;
    std::unordered_map<std::string, std::size_t> typedefIndex_;
    std::vector<EnumConst> enums_;
    std::unordered_map<std::string, std::size_t> enumIndex_;
    int strings_ = 0;
    int loopDepth_ = 0;
    int switchDepth_ = 0;
    int caseIds_ = 0;

    struct SwitchCtx {
        std::vector<const Case *> cases;
        const Case *deflt;
        const Type *governing;
    };
    std::vector<SwitchCtx> switches_;

    struct LabelDef { std::string name; std::size_t pos; };
    std::vector<LabelDef> labels_;
    std::vector<LabelDef> gotos_;

    const Token &peek() const { return tokens_[at_]; }
    const Token &peekAt(std::size_t n) const;
    bool consume(const char *s);
    void expect(const char *s);
    std::string expectIdent(const char *what);
    long expectNumber(const char *what);

    bool atTypeName() const;
    const Type *findTypedef(const std::string &name) const;
    const EnumConst *findEnum(const std::string &name) const;
    const Type *structOrUnionSpecifier(Kind kind);
    const Type *enumSpecifier();
    bool atDeclarationStart() const;
    const Type *specifiers(StorageClass *storage, Qualifiers *quals = nullptr);
    Declared declarator(const Type *base, bool nameOptional = false);
    const Type *arraySuffix(const Type *base, std::size_t pos);
    const Type *promote(const Type *t) const;
    const Type *usualArithmetic(const Type *a, const Type *b) const;
    ExprPtr convert(ExprPtr e, const Type *to) const;
    const Type *unsignedVersion(const Type *t) const;

    ExprPtr decay(ExprPtr e);
    void requireScalar(const Expr &e, std::size_t pos, const char *what);

    void checkAssignable(const Expr &from, const Type *to, std::size_t pos,
                         const std::string &what) const;

    int declare(const std::string &name, const Type *type, std::size_t pos);
    int allocateFrameSlot(const Type *type);
    void declareStaticLocal(const std::string &name, const Type *type,
                            std::size_t pos, const std::string &symbol);
    void requireAssignable(const Expr &e, std::size_t pos, const char *what);
    const Local *findLocal(const std::string &name) const;
    void enterScope();
    void leaveScope();
    const GlobalSym *findGlobal(const std::string &name) const;
    GlobalSym *findGlobalToUpdate(const std::string &name);
    void declareFunction(const std::string &name, const Type *returns,
                         const std::vector<const Type *> &params,
                         bool variadic, bool defining, std::size_t pos);
    ExprPtr defaultPromote(ExprPtr e);
    const Signature &lookupFunction(const std::string &name, std::size_t pos) const;
    const Signature *findFunction(const std::string &name) const;

    void parameterTypes(std::vector<const Type *> &params, bool &variadic);

    ExprPtr finishCall(const std::string &name, ExprPtr callee, const Type *returns,
                       const std::vector<const Type *> &params, bool variadic,
                       std::size_t pos);

    ExprPtr objectRef(const std::string &name);

    struct Init {
        bool isList = false;
        ExprPtr value;
        std::vector<Init> items;
        std::size_t pos = 0;
    };

    struct InitStep {
        const Member *member = nullptr;
        long index = 0;
    };

    Init parseInitialiser();
    ExprPtr targetFor(const std::string &name, const std::vector<InitStep> &path);
    void emitInit(const std::string &name, std::vector<InitStep> &path,
                  const Type *type, Init &in, std::vector<StmtPtr> &out);
    void flattenInit(const Type *type, Init &in, int base,
                     std::vector<GlobalPiece> &out);
    long inferredLength(const Init &in, const Type *element, std::size_t pos);
    static const StrLit *stringInitialiser(const Init &in, const Type *type);

    void topLevel(Program &program);
    StmtPtr block();
    StmtPtr statement();
    StmtPtr forStatement();
    StmtPtr switchStatement();
    StmtPtr caseLabel();
    StmtPtr gotoLabel();
    StmtPtr declaration();
    void resolveGotos();

    long constantExpression(const char *what);
    bool fold(const Expr &e, long *out, std::size_t pos) const;
    long narrowTo(long v, const Type *t) const;

    ExprPtr expr();
    ExprPtr assign();
    ExprPtr conditional();
    ExprPtr bitOr();
    ExprPtr bitXor();
    ExprPtr bitAnd();
    ExprPtr compound(BinOp op, ExprPtr target, ExprPtr value, std::size_t pos);
    ExprPtr incDec(ExprPtr target, bool increment, bool prefix, std::size_t pos);
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

    Program *current_ = nullptr;
};
