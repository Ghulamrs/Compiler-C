#include "X86_64Windows.h"
#include "X86_64Linux.h"
#include "Masm.h"

#include <cstdio>
#include <cstdlib>

int WindowsX86_64Target::sizeOf(Kind k) const {
    switch (k) {
    case Kind::Void:                                       return 1;
    case Kind::Char: case Kind::SChar: case Kind::UChar:   return 1;
    case Kind::Short: case Kind::UShort:                   return 2;
    case Kind::Int: case Kind::UInt:                       return 4;
    case Kind::Long: case Kind::ULong:                     return 4;
    case Kind::LongLong: case Kind::ULongLong:             return 8;
    case Kind::Float:                                      return 4;
    case Kind::Double:                                     return 8;

    case Kind::LongDouble:                                 return 8;
    case Kind::Pointer:                                    return 8;
    default:
        std::fprintf(stderr, "target: no size for this type yet\n");
        std::exit(1);
    }
}

int WindowsX86_64Target::alignOf(Kind k) const { return sizeOf(k); }

static const char *const kArgRegs[] = { "%rcx", "%rdx", "%r8", "%r9" };
static const char *const kSseRegs[] = { "%xmm0", "%xmm1", "%xmm2", "%xmm3" };

static const Abi kMsAbi = {
    kArgRegs, 4,
    kSseRegs, 4,
    true,
    32,
    8,
    true,
    false,
    "%r10", "%r10d",
    false,
    false,

};

const Abi &X86_64WindowsBackend::abi() const { return kMsAbi; }

static bool gnuSyntax_ = false;
void setWindowsAsmSyntax(bool gnu) { gnuSyntax_ = gnu; }

static const char *const kWindowsMacros[] = {
    "__x86_64__=1", "__x86_64=1", "__amd64__=1", "__amd64=1",
    "_WIN32=1", "_WIN64=1", "__llp64__=1", nullptr,
};
const char *const *X86_64WindowsBackend::identityMacros() const { return kWindowsMacros; }

bool X86_64WindowsBackend::emitsLineTable() const { return gnuSyntax_; }

std::unique_ptr<CodeGen> X86_64WindowsBackend::codegen(std::ostream &sink) const {
    if (gnuSyntax_)
        return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, kMsAbi));
    return std::unique_ptr<CodeGen>(new MasmCodeGen(sink, target_, kMsAbi));
}
