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
// Not a second code generator: every instruction cc1 selects for Windows is
// the one it selects for Linux, and only the spelling differs - operands the
// other way round, no sigils, '[rbp-4]' for '-4(%rbp)', 'DWORD PTR' where GNU
// puts the size in the mnemonic's suffix.
//
// This was a translation until the generator learned to emit structurally: the
// second pass cost 3.8x the whole compile, and it meant the compiler
// recognising its own output rather than knowing it. Anything it is asked for
// that it does not know stops the compiler.
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
    // A data label waits for its datum: GNU writes the label above the data and
    // MASM defines the two together as 'name DB ...'.
    std::string pending_;
    // Known first-hand from the generator's calls, where the translation had to
    // rediscover them by scanning its own output.
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

class MasmCodeGen final : public X86_64Linux {
public:
    MasmCodeGen(std::ostream &sink, const Target &target, const Abi &abi)
        : X86_64Linux(sink, target, abi), masm_(out_) { a_ = &masm_; }

private:
    MasmSpelling masm_;
};
