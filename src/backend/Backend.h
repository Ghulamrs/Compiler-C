#pragma once

#include "../Ast.h"
#include "../Type.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Source;

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;

    virtual void setLineSource(const Source *, const std::string &) {}
};

enum class Segment { Code, Const, ConstRelocated, Data, Bss };

Segment segmentFor(const Global &g);

struct Abi {
    const char *const *intRegs;
    int intCount;
    const char *const *sseRegs;
    int sseCount;

    bool positional;

    int shadowBytes;

    int structReturnLimit;

    bool aggregatesByReference;

    bool variadicSseCountInAl;

    const char *scratch;
    const char *scratch32;

    bool homogeneousFloatAggregates;

    bool elfSymbolAttributes;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char *name() const = 0;
    virtual const Target &target() const = 0;
    virtual const Abi &abi() const = 0;

    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink) const = 0;
    virtual bool emits() const = 0;

    virtual bool emitsLineTable() const { return false; }

    virtual const char *const *identityMacros() const = 0;
};

std::vector<std::pair<std::string, std::string> > predefinedMacros(const Backend &b);

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
