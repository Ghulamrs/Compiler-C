#pragma once

#include "Backend.h"
#include "Spelling.h"
#include "X86_64Linux.h"

#include <iosfwd>
#include <set>
#include <string>
#include <vector>

// The Microsoft assembler reads a different language from GNU as, and this is
// that language, written first-hand.
//
// Not a second code generator. Every instruction cc1 selects for Windows is
// the instruction it selects for Linux - the Abi is the whole of what differs,
// which is the point of holding a calling convention as data - so a second
// emitter would be the same selection written twice and would drift. What
// differs here is only how an instruction is *spelled*: operands the other way
// round, no '%' or '$' sigils, '[rbp-4]' for '-4(%rbp)', a size named as
// 'DWORD PTR' where GNU names it in the mnemonic's suffix, and segments and
// symbol declarations that MASM wants stated rather than inferred.
//
// This used to be a translation: the generator wrote AT&T text into a buffer
// and a second pass re-parsed every line of it to respell it. That pass cost
// 3.8x the whole compile on a large project, and it meant the compiler
// recognising its own output rather than knowing it - EXTERN and PUBLIC were
// found by scanning the text for labels, and the unwind data by reading the
// prologue back and refusing any shape it did not expect. Now the generator's
// structured emission calls - see Spelling.h - arrive here directly, and each
// is written out in MASM once, knowing what its operands are.
//
// Anything the generator asks for that this spelling does not know stops the
// compiler with a message naming it. Silence would mean emitting an
// instruction that assembles into something other than what was meant, which
// is the one failure this file must not have.
class MasmSpelling final : public Spelling {
public:
    explicit MasmSpelling(std::string &o) : o_(o) {}

    void ins(const std::string &m) override;
    void ins(const std::string &m, const Op &a) override;
    void ins(const std::string &m, const Op &a, const Op &b) override;

    void defLabel(const std::string &l) override;
    void functionBegin(const std::string &name, bool exported) override;
    void prologue(int frameSize) override;
    void functionEnd(const std::string &name) override;

    void globl(const std::string &name) override;
    void textSection() override;
    void rodataSection() override;
    void dataSection() override;
    void bssSection() override;
    void objectType(const std::string &name) override;
    void objectSize(const std::string &name, int size) override;
    void align(int n) override;
    void zero(int n) override;
    void dataInt(int size, long long v) override;
    void dataSym(const std::string &sym, long long off) override;
    void dataBytes(const std::string &bytes) override;

    void predefine(const std::vector<std::string> &names) override;
    void preamble(std::ostream &sink) override;
    void postamble(std::ostream &sink) override;

private:
    std::string &o_;
    enum Seg { None, Code, Data, Const, Bss } seg_ = None;
    // A data label waits for its datum, because GNU writes the label above the
    // data and MASM defines the two together as 'name DB ...'.
    std::string pending_;
    // What this unit defines, exports and references - known first-hand from
    // the generator's own calls, where the translation had to rediscover them
    // by scanning its output for labels. The preamble is derived from these
    // once emission is over, which works because the body rides in the same
    // per-function chunks the GNU path already collects for parallelism.
    std::set<std::string> defined_, exported_, referenced_, unreserved_;

    struct Rendered {
        std::string text;
        bool isMem = false, isImm = false, isXmm = false;
    };
    Rendered render(const Op &x);
    std::string mangle(const std::string &name);
    void flushPending();
    void items(const char *dir, const std::vector<std::string> &it);
};

// The generator, spelling its output in MASM as it goes.
class MasmCodeGen final : public X86_64Linux {
public:
    MasmCodeGen(std::ostream &sink, const Target &target, const Abi &abi)
        : X86_64Linux(sink, target, abi), masm_(out_) { a_ = &masm_; }

private:
    MasmSpelling masm_;
};
