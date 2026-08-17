#pragma once

#include "../Ast.h"
#include "../Type.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;
};

// The four segments every object format has, whatever it spells them.
enum class Segment { Code, Const, Data, Bss };

// Const is asked first and deliberately: a const object with an initialiser
// belongs in read-only data, not in .data.
Segment segmentFor(const Global &g);

// A calling convention as data, so one x86-64 generator serves System V and
// Microsoft x64.
struct Abi {
    const char *const *intRegs;   // argument registers, in the order they fill
    int intCount;
    const char *const *sseRegs;
    int sseCount;

    bool positional;      // argument n takes slot n in whichever file, and
                          // spending one file's slot spends the other's
    int shadowBytes;      // the caller leaves this much for the callee to spill
                          // its register arguments into, below the return address
    int structReturnLimit;      // wider than this and a struct comes back
                                // through a pointer the caller supplies
    bool aggregatesByReference; // an oversized aggregate travels as a pointer to
                                // the caller's copy, not copied onto the stack
    bool variadicSseCountInAl;  // %al carries the vector count a variadic
                                // callee reads

    // Call-clobbered under System V but callee-saved under Microsoft x64, so the
    // two conventions cannot share one scratch register.
    const char *scratch;
    const char *scratch32;

    // AAPCS64 sends a homogeneous float aggregate in that many vector registers
    // whatever its size.
    bool homogeneousFloatAggregates;

    // Not a property of the calling convention: ELF records a symbol's type and
    // size, and clang targeting PE rejects '@object' outright.
    bool elfSymbolAttributes;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char *name() const = 0;
    virtual const Target &target() const = 0;
    virtual const Abi &abi() const = 0;

    // Null until the instructions for this platform are written.
    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink) const = 0;
    virtual bool emits() const = 0;

    // What this platform calls itself, as "NAME=VALUE" strings.
    virtual const char *const *identityMacros() const = 0;
};

std::vector<std::pair<std::string, std::string> > predefinedMacros(const Backend &b);

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
