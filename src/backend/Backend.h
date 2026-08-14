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

// A calling convention as data. The x86-64 instruction selection is the same on
// Linux and Windows down to the mnemonics; everything that differs between them
// is here, so there is one code generator rather than two nearly identical ones.
struct Abi {
    const char *const *intRegs;   // argument registers, in the order they fill
    int intCount;
    const char *const *sseRegs;
    int sseCount;

    // System V counts the two register files independently, so a call can run
    // out of integer registers while SSE ones remain. Windows numbers argument
    // slots: the third argument takes the third slot, %r8 or %xmm2 by its
    // position, and using one spends the other.
    bool positional;

    // Windows requires the caller to leave 32 bytes below the return address
    // for the callee to spill its register arguments into. System V has no
    // such area and starts its memory arguments at 16(%rbp).
    int shadowBytes;

    // The widest struct returned in registers; over this it travels through a
    // pointer the caller supplies. 16 under System V, 8 under Windows x64.
    int structReturnLimit;

    // How an aggregate too big for a register travels. System V copies it onto
    // the stack as part of the argument block; Windows passes a pointer to a
    // copy the caller made, which is a different mechanism and not a tuning.
    bool aggregatesByReference;

    // %al carries the number of vector registers used, which a variadic callee
    // reads. System V only.
    bool variadicSseCountInAl;
};

// One platform: what its types measure, what its ABI decides, and how to make
// the code generator that emits for it. A backend is a Target plus a CodeGen,
// and separating them is what lets the type answers exist before the
// instructions do - x86_64-windows can measure sizeof(long) as 4 and still have
// nothing that can emit a function.
class Backend {
public:
    virtual ~Backend() = default;

    virtual const char *name() const = 0;
    virtual const Target &target() const = 0;
    virtual const Abi &abi() const = 0;

    // Null until the instructions for this platform are written. A backend that
    // can measure but not emit is a real state, and saying so is better than
    // pretending the target does not exist.
    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink) const = 0;
    virtual bool emits() const = 0;
};

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
