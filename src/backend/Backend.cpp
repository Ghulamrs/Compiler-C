#include "Arm64Darwin.h"
#include "Backend.h"
#include "X86_64Linux.h"
#include "X86_64Windows.h"

#include <ostream>
#include <string>

std::unique_ptr<CodeGen> X86_64LinuxBackend::codegen(std::ostream &sink) const {
    return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, abi()));
}

static const X86_64LinuxBackend kLinux;
static const X86_64WindowsBackend kWindows;
static const Arm64DarwinBackend kDarwin;

static const Backend *const kBackends[] = { &kLinux, &kWindows, &kDarwin };

std::vector<std::pair<std::string, std::string> > predefinedMacros(const Backend &b) {
    const Target &t = b.target();
    std::vector<std::pair<std::string, std::string> > out;
    auto add = [&out](const std::string &k, const std::string &v) {
        out.push_back(std::make_pair(k, v));
    };
    auto width = [&t](Kind k) { return std::to_string(t.sizeOf(k)); };

    add("__STDC__", "1");
    add("__STDC_HOSTED__", "1");
    add("__CHAR_BIT__", "8");
    add("__SIZEOF_SHORT__", width(Kind::Short));
    add("__SIZEOF_INT__", width(Kind::Int));
    add("__SIZEOF_LONG__", width(Kind::Long));
    add("__SIZEOF_LONG_LONG__", width(Kind::LongLong));
    add("__SIZEOF_FLOAT__", width(Kind::Float));
    add("__SIZEOF_DOUBLE__", width(Kind::Double));
    add("__SIZEOF_POINTER__", width(Kind::Pointer));

    for (const char *const *m = b.identityMacros(); *m != nullptr; m++) {
        std::string s(*m);
        std::size_t eq = s.find('=');
        add(s.substr(0, eq), s.substr(eq + 1));
    }
    return out;
}

Segment segmentFor(const Global &g) {
    if (g.isConst) return Segment::Const;
    if (!g.hasInit) return Segment::Bss;
    // An initialiser that is all zeroes leaves nothing worth carrying, so it goes
    // to .bss - where the size is recorded and no bytes reach the object file.
    for (const GlobalPiece &p : g.init)
        if (p.value != 0 || !p.symbol.empty()) return Segment::Data;
    return Segment::Bss;
}

const Backend *findBackend(const std::string &name) {
    for (const Backend *b : kBackends)
        if (name == b->name()) return b;
    return nullptr;
}

// The host it was built on, rather than one fixed choice: a compiler whose
// default output its own machine cannot assemble looks broken at step one.
const Backend &defaultBackend() {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    return kDarwin;
#elif defined(_WIN32)
    return kWindows;
#else
    return kLinux;
#endif
}

std::string backendNames() {
    std::string all;
    for (const Backend *b : kBackends) {
        if (!all.empty()) all += ", ";
        all += b->name();
    }
    return all;
}
