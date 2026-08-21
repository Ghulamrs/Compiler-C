#include "Arm64Darwin.h"
#include "Backend.h"
#include "X86_64Linux.h"
#include "X86_64Windows.h"

#include <ctime>
#include <ostream>
#include <string>

std::unique_ptr<CodeGen> X86_64LinuxBackend::codegen(std::ostream &sink) const {
    return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, abi()));
}

static const X86_64LinuxBackend kLinux;
static const X86_64WindowsBackend kWindows;
static const Arm64DarwinBackend kDarwin;

static const Backend *const kBackends[] = { &kLinux, &kWindows, &kDarwin };

// __DATE__ and __TIME__, the two of C90's five predefined macros that were not
// here - "Aug 22 2026" and "14:03:09", both as string literals, and both the
// same for every use in one translation unit because they are worked out once
// and seeded like any other macro.
//
// Built from the fields rather than through strftime, for two reasons. %b is
// locale-dependent and the standard names the English abbreviations, so a
// machine in another locale would spell __DATE__ in a way no C program expects.
// And %e, which pads the day with a space as C requires, is not in every
// strftime this is built against - MSVC has no such conversion.
static std::string twoDigits(int n) {
    const std::string digits = std::to_string(n);
    return digits.size() < 2 ? "0" + digits : digits;
}

static void addTranslationTime(std::vector<std::pair<std::string, std::string> > &out) {
    static const char *const kMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    const std::time_t now = std::time(nullptr);
    const std::tm *when = std::localtime(&now);
    if (when == nullptr) return;   // a clock that will not answer is not a reason to stop

    // "Mmm dd yyyy", where a day below ten is padded with a space and not a
    // zero. That is what C says, and it is the half of the format that is
    // always got wrong.
    const std::string day = std::to_string(when->tm_mday);
    std::string date = kMonths[when->tm_mon % 12];
    date += " ";
    date += (day.size() < 2 ? " " + day : day);
    date += " " + std::to_string(when->tm_year + 1900);

    const std::string time = twoDigits(when->tm_hour) + ":" + twoDigits(when->tm_min) +
                             ":" + twoDigits(when->tm_sec);

    out.push_back(std::make_pair("__DATE__", "\"" + date + "\""));
    out.push_back(std::make_pair("__TIME__", "\"" + time + "\""));
}

std::vector<std::pair<std::string, std::string> > predefinedMacros(const Backend &b) {
    const Target &t = b.target();
    std::vector<std::pair<std::string, std::string> > out;
    auto add = [&out](const std::string &k, const std::string &v) {
        out.push_back(std::make_pair(k, v));
    };
    auto width = [&t](Kind k) { return std::to_string(t.sizeOf(k)); };

    add("__STDC__", "1");
    add("__STDC_HOSTED__", "1");
    addTranslationTime(out);
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

// Whether the initialiser names something rather than only holding numbers.
// A piece with a symbol in it is an address, and an address is a relocation.
static bool relocates(const Global &g) {
    for (const GlobalPiece &p : g.init)
        if (!p.symbol.empty()) return true;
    return false;
}

Segment segmentFor(const Global &g) {
    if (g.isConst) return relocates(g) ? Segment::ConstRelocated : Segment::Const;
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
