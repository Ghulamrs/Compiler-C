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
    // Microsoft made 'long double' a synonym for double when it dropped x87, and
    // the UCRT's printf reads '%Lf' as eight bytes to match.
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
    true,    // positional: argument n takes slot n in whichever file
    32,      // shadow space the caller leaves for the callee to spill into
    8,       // a struct over 8 bytes comes back through a hidden pointer
    true,    // an oversized aggregate is passed as a pointer to the caller's copy
    false,   // no %al convention; a variadic callee reads its own shadow space
    "%r10", "%r10d",  // not %rdi, which this ABI makes the caller's to get back
    false,
    false,   // PE/COFF: clang refuses '@object' outright, and MASM has no use
             // for either directive
};

const Abi &X86_64WindowsBackend::abi() const { return kMsAbi; }

static bool gnuSyntax_ = false;
void setWindowsAsmSyntax(bool gnu) { gnuSyntax_ = gnu; }

// _WIN32 is defined on 64-bit Windows too and is not a mistake: _WIN64 is what
// separates them.
static const char *const kWindowsMacros[] = {
    "__x86_64__=1", "__x86_64=1", "__amd64__=1", "__amd64=1",
    "_WIN32=1", "_WIN64=1", "__llp64__=1", nullptr,
};
const char *const *X86_64WindowsBackend::identityMacros() const { return kWindowsMacros; }

// Whether -g can be honoured here is a question about the *spelling*, not
// about the target, and asking the backend alone got it wrong for as long as
// this refused -g outright.
//
// '-masm=gnu' hands the job to X86_64Linux, which has written a line table,
// types and objects all along - the Abi is the whole of what differs, and
// none of the DWARF depends on the Abi. So the Microsoft ABI, LLP64 widths
// and all, is debuggable in the GNU spelling, and gdb reads it: the suite
// runs the debug corpus against this target the same way tests/windows.sh
// runs the ordinary one, and for the same reason.
//
// MASM is still no, and for a reason that has not moved: ml64 builds no line
// table, and what a native Windows debugger wants is CodeView in .debug$S
// and .debug$T rather than DWARF in any spelling. That is a format to write,
// not a flag to set.
bool X86_64WindowsBackend::emitsLineTable() const { return gnuSyntax_; }

// The same generator the Linux backend uses - the Abi is the whole of what
// differs - spelling its output in MASM. '-masm=gnu' writes the GNU form,
// which is what the suites that assemble with gcc and clang pass.
std::unique_ptr<CodeGen> X86_64WindowsBackend::codegen(std::ostream &sink) const {
    if (gnuSyntax_)
        return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, kMsAbi));
    return std::unique_ptr<CodeGen>(new MasmCodeGen(sink, target_, kMsAbi));
}
