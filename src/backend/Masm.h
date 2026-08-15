#pragma once

#include "Backend.h"
#include "X86_64Linux.h"

#include <iosfwd>
#include <sstream>
#include <string>

// The Microsoft assembler reads a different language from the one the generator
// writes, and this is the translation between them.
//
// Not a second code generator. Every instruction cc1 selects for Windows is the
// instruction it selects for Linux - the Abi is the whole of what differs, which
// is the point of holding a calling convention as data - so a second emitter
// would be the same selection written twice and would drift. What differs here
// is only how an instruction is *spelled*: operands the other way round, no '%'
// or '$' sigils, '[rbp-4]' for '-4(%rbp)', a size named as 'DWORD PTR' where
// GNU names it in the mnemonic's suffix, and segments and symbol declarations
// that MASM wants stated rather than inferred.
//
// So the generator writes what it always writes, into a buffer, and this
// rewrites the buffer. One vocabulary, translated once, checked against the
// whole corpus by assembling it with ml64.
//
// Anything unrecognised stops the compiler with a message naming the line.
// Silence would mean emitting an instruction that assembles into something
// other than what was meant, which is the one failure this file must not have.
void attToMasm(const std::string &att, std::ostream &out);

// The generator, with its output redirected through the translation above.
// Buffer first: it is a base rather than a member so that it is alive before
// X86_64Linux is handed a reference to it.
struct MasmBuffer { std::ostringstream buf; };

class MasmCodeGen final : private MasmBuffer, public X86_64Linux {
public:
    MasmCodeGen(std::ostream &sink, const Target &target, const Abi &abi)
        : MasmBuffer(), X86_64Linux(buf, target, abi), sink_(sink) {}

    void run(const Program &program) override {
        X86_64Linux::run(program);
        attToMasm(buf.str(), sink_);
    }

private:
    std::ostream &sink_;
};
