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

    // The largest struct this ABI returns in registers. Anything wider travels
    // through a pointer the caller supplies, which the parser has to know about
    // because only the parser can reserve the frame slot. System V and AAPCS64
    // say 16; Windows x64 says 8.
    virtual int structReturnLimit() const = 0;

    // Null until the instructions for this platform are written. A backend that
    // can measure but not emit is a real state, and saying so is better than
    // pretending the target does not exist.
    virtual std::unique_ptr<CodeGen> codegen(std::ostream &sink) const = 0;
    virtual bool emits() const = 0;
};

const Backend *findBackend(const std::string &name);
const Backend &defaultBackend();
std::string backendNames();
