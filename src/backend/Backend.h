#pragma once

#include "../Ast.h"
#include "../Type.h"

#include <iosfwd>
#include <memory>
#include <string>

class CodeGen : public Visitor {
public:
    ~CodeGen() override = default;
    virtual void run(const Program &program) = 0;
};

// A calling convention as data, so one x86-64 generator serves System V and
// Microsoft x64 rather than two files that are ninety per cent the same. The
// three conventions are set side by side in docs/STATUS.md; what each field
// means to the generator is below.
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

    // The right-hand operand of a binary and the address a store writes
    // through. A field rather than a literal because the generator destroys it
    // between statements, so it has to be call-clobbered - which %rdi is under
    // System V and is not under Windows.
    const char *scratch;
    const char *scratch32;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char *name() const = 0;
    virtual const Target &target() const = 0;
    virtual const Abi &abi() const = 0;

    // Null until the instructions for this platform are written: measuring
    // without emitting is a real state, and the driver says so by name.
    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink) const = 0;
    virtual bool emits() const = 0;
};

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
