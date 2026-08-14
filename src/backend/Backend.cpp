#include "Arm64Darwin.h"
#include "Backend.h"
#include "X86_64Linux.h"
#include "X86_64Windows.h"

#include <ostream>

std::unique_ptr<CodeGen> X86_64LinuxBackend::codegen(std::ostream &sink) const {
    return std::unique_ptr<CodeGen>(new X86_64Linux(sink, target_, abi()));
}

static const X86_64LinuxBackend kLinux;
static const X86_64WindowsBackend kWindows;
static const Arm64DarwinBackend kDarwin;

static const Backend *const kBackends[] = { &kLinux, &kWindows, &kDarwin };

const Backend *findBackend(const std::string &name) {
    for (const Backend *b : kBackends)
        if (name == b->name()) return b;
    return nullptr;
}

const Backend &defaultBackend() { return kLinux; }

std::string backendNames() {
    std::string all;
    for (const Backend *b : kBackends) {
        if (!all.empty()) all += ", ";
        all += b->name();
    }
    return all;
}
