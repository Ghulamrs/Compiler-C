#include "Driver.h"
#include "backend/X86_64Windows.h"

#include "backend/Backend.h"
#include "Lexer.h"
#include "Parser.h"
#include "Preprocessor.h"
#include "Source.h"
#include "Type.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include <unistd.h>

#ifdef __linux__
#include <sched.h>
#endif

namespace {

const std::size_t kThreadFrom = 4;

}

#ifndef CC1_INCLUDE_DIR
#define CC1_INCLUDE_DIR ""
#endif

// **1.1 is the first version this compiler has had a number for.** It had none
// until 2026-08-26 - built, relayed between three machines and run without one,
// which works until somebody holds two copies and has to say which is which.
//
// Numbered with the group rather than on its own. cc1 is used beside a
// particular RStudio and shc, and 1.1 is the release where it stopped being
// the only compiler in the workbench: a target could hold C and C++ together,
// and C became the one language with a choice of compiler in it. The 1.2 work
// is Shalimar's; cc1's part in it was to build the libraries a Shalimar
// program calls, which asked nothing new of the compiler itself.
const char *cc1Version() { return "1.1"; }

void Driver::usage(char *file) {
    std::fprintf(stderr,
        "cc1 %s - an ANSI C compiler\n"
        "\n"
        "usage: %s <file.c> [more.c ...] [-S|-c] [-o out] [-D n[=v]] [-U n]\n"
        "               [-I dir] [-j n] [-arch a] [-masm=m] [-g] [-time]\n"
        "       with neither -S nor -c the inputs are compiled, assembled and\n"
        "         linked into a program, named by -o, or a.out - a.exe on a\n"
        "         Windows host; several inputs\n"
        "         link together\n"
        "       -c stops at one object file per input, named by -o or after the\n"
        "         input, in the current directory\n"
        "       -S stops after this compiler and writes assembly instead: one .s\n"
        "         per input, or -o to name the output of a single one\n"
        "       -D defines a macro, '-DN' meaning '-DN=1'; -U removes one, and\n"
        "         either may name one of the target's own\n"
        "       -I adds a directory to the ones <...> searches\n"
        "       -j sets how many files are compiled at once; -j 1 is serial\n"
        "       -arch picks the architecture the code is generated for - one of\n"
        "         x86_64-linux, x86_64-windows, arm64-darwin; the host by default,\n"
        "         and another one only reaches -S, since the assembler here is\n"
        "         this machine's\n"
        "       -masm picks the assembly syntax for x86_64-windows: 'masm' for\n"
        "         ml64, which is the default, or 'gnu' for the GNU spelling\n"
        "       -g writes a line table, so a debugger can stop on a line of C\n"
        "         and step through it; x86_64-linux and arm64-darwin only\n"
        "       -time reports how long each phase took\n", cc1Version(), file);
}

static std::string workingDirectory() {
    char buf[4096];
    if (getcwd(buf, sizeof buf) == nullptr) return std::string();
    return std::string(buf);
}

static bool hostIsWindows() {
    return std::strcmp(defaultBackend().name(), "x86_64-windows") == 0;
}

#ifdef _WIN32
// vswhere's answer, fetched through a temporary file rather than a pipe.
//
// _popen would be the obvious way and does not work here. cc1 is itself run
// through a pipe by the editor, and a nested _popen fails when the parent's
// stdio are not consoles - so this found Visual Studio from a command prompt
// and never from inside RStudio, which is the one place it was needed.
static std::string askVswhere() {
    char temp[MAX_PATH];
    char folder[MAX_PATH];
    if (GetTempPathA(MAX_PATH, folder) == 0) return std::string();
    if (GetTempFileNameA(folder, "cc1", 0, temp) == 0) return std::string();

    std::string command =
        "\"\"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe\""
        " -latest -products * -property installationPath > \"";
    command += temp;
    command += "\"\"";
    std::string found;
    if (std::system(command.c_str()) == 0) {
        std::ifstream answer(temp);
        std::getline(answer, found);
    }
    std::remove(temp);
    while (!found.empty() && (found.back() == '\n' || found.back() == '\r')) {
        found.pop_back();
    }
    return found;
}

static std::string findVcvars() {
    std::string root = askVswhere();
    if (!root.empty()) {
        const std::string bat = root + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
        std::ifstream there(bat.c_str());
        if (there) return bat;
    }
    // vswhere is itself part of an installation and can be absent. The
    // default places are worth trying before giving up on a machine that
    // plainly has the tools.
    static const char *const roots[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\"
    };
    static const char *const editions[] = {
        "Community", "Professional", "Enterprise", "BuildTools"
    };
    for (std::size_t r = 0; r < sizeof roots / sizeof roots[0]; ++r) {
        for (std::size_t e = 0; e < sizeof editions / sizeof editions[0]; ++e) {
            const std::string bat = std::string(roots[r]) + editions[e] +
                                    "\\VC\\Auxiliary\\Build\\vcvars64.bat";
            std::ifstream there(bat.c_str());
            if (there) return bat;
        }
    }
    return std::string();
}
#endif

// ml64 and link live in Visual Studio and are on PATH only inside a Developer
// Command Prompt. An editor launched from Explorer is not one, so every build
// it asked cc1 for failed at the assembler with a message telling a person to
// open a different shell - which the editor cannot do for them.
//
// So when the tools are not already reachable, the command is run inside a
// shell that has sourced vcvars64.bat. That sets LIB as well as PATH, which
// matters: finding ml64 alone still leaves the linker unable to see
// libcmt.lib. Empty when there is nothing to add, and the command runs as it
// always did.
static std::string developerShell() {
#ifdef _WIN32
    const char *inside = std::getenv("VCToolsInstallDir");
    if (inside != nullptr && inside[0] != '\0') return std::string();
    static bool asked = false;
    static std::string cached;
    if (!asked) { asked = true; cached = findVcvars(); }
    return cached;
#else
    return std::string();
#endif
}

static std::string lastToolCommand;

// Runs one tool, inside a developer environment when the machine needs one.
//
// Through a batch file rather than by prefixing the command. cmd's rule for
// stripping the outer quotes of a /c string is not something to build on when
// the string already holds several quoted paths and an '&&' - every spelling
// tried produced "The filename, directory name, or volume label syntax is
// incorrect" from somewhere inside it. A file has no quoting question: the
// call is on its own line and the command is on the next, exactly as written.
// cmd /c strips the first and last quote of a command that has both, so a
// command whose program is quoted and whose last argument is quoted arrives
// mangled - '"ml64.exe" ... "x.s"' becomes 'ml64.exe" ... "x.s'. One more
// pair around the whole thing is what cmd eats instead. This is only visible
// where the tools are already on PATH and no vcvars shell is added, which is
// how it hid: standalone the wrapper replaced the command and covered it,
// and the editor - which imports the MSVC environment into itself before
// running anything - was the one caller that took this path.
static std::string forCmd(const std::string &command) {
#ifdef _WIN32
    return "\"" + command + "\"";
#else
    return command;
#endif
}

static int runTool(const std::string &command) {
    lastToolCommand = command;
    const std::string vcvars = developerShell();
    if (vcvars.empty()) return std::system(forCmd(command).c_str());

#ifdef _WIN32
    char folder[MAX_PATH];
    char script[MAX_PATH];
    if (GetTempPathA(MAX_PATH, folder) == 0) return std::system(forCmd(command).c_str());
    if (GetTempFileNameA(folder, "cc1", 0, script) == 0) {
        return std::system(forCmd(command).c_str());
    }
    std::string batch = script;
    std::remove(batch.c_str());
    batch += ".cmd";

    {
        std::ofstream out(batch.c_str());
        if (!out) return std::system(forCmd(command).c_str());
        out << "@echo off\n";
        out << "call \"" << vcvars << "\" >nul 2>&1\n";
        out << command << "\n";
    }
    lastToolCommand = command + "   [inside " + vcvars + "]";
    const int rc = std::system(("\"" + batch + "\"").c_str());
    std::remove(batch.c_str());
    return rc;
#else
    return std::system(command.c_str());
#endif
}

static void noteWindowsToolchain() {
    if (!hostIsWindows()) return;
    std::fprintf(stderr, "  ml64 and link ship with Visual Studio and reach "
                         "PATH only after vcvars64.bat has run - a Developer "
                         "Command Prompt is that same environment.\n");
}

const char *Driver::hostCompiler() {
    const char *env = std::getenv("CC1_CC");
    return (env != nullptr && env[0] != '\0') ? env : "cc";
}

const char *Driver::hostAssembler() {
    const char *env = std::getenv("CC1_AS");
    return (env != nullptr && env[0] != '\0') ? env : "ml64.exe";
}

const char *Driver::hostLinker() {
    const char *env = std::getenv("CC1_LD");
    return (env != nullptr && env[0] != '\0') ? env : "link.exe";
}

std::string Driver::temporaryName(int index) {
    const char *dir = std::getenv("TMPDIR");
    if (dir == nullptr || dir[0] == '\0') dir = std::getenv("TEMP");
    if (dir == nullptr || dir[0] == '\0') dir = std::getenv("TMP");
    std::string base = (dir != nullptr && dir[0] != '\0') ? dir : "/tmp";
    while (!base.empty() && (base[base.size() - 1] == '/' ||
                             base[base.size() - 1] == '\\'))
        base.erase(base.size() - 1);
    return base + (hostIsWindows() ? "\\" : "/") + "cc1-" +
           std::to_string(static_cast<long>(getpid())) + "-" +
           std::to_string(index) + ".s";
}

static std::vector<std::string> &temporaryNames() {
    static std::vector<std::string> names;
    return names;
}

void Driver::removeTemporaries() {
    std::vector<std::string> &names = temporaryNames();
    for (const std::string &t : names) std::remove(t.c_str());
    names.clear();
    temporaries_.clear();
}

static std::string shellQuote(const std::string &s) {
    if (hostIsWindows()) {
        std::string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else          out += c;
        }
        return out + "\"";
    }
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    return out + "'";
}

void Driver::addMacroEdit(const char *text, bool undef) {
    std::string s(text);
    std::size_t eq = s.find('=');
    if (undef || eq == std::string::npos)
        macroEdits_.push_back(MacroEdit{ s, undef ? "" : "1", undef });
    else
        macroEdits_.push_back(MacroEdit{ s.substr(0, eq), s.substr(eq + 1), false });
}

std::vector<std::pair<std::string, std::string> > Driver::macrosFor() const {
    std::vector<std::pair<std::string, std::string> > macros =
        predefinedMacros(*backend_);

    for (const MacroEdit &e : macroEdits_) {
        for (std::size_t i = macros.size(); i-- > 0; )
            if (macros[i].first == e.name)
                macros.erase(macros.begin() + static_cast<long>(i));
        if (!e.undef) macros.push_back(std::make_pair(e.name, e.value));
    }
    return macros;
}

bool Driver::assembleObjects() {
    for (std::size_t i = 0; i < temporaries_.size(); i++) {
        std::string command;
        if (hostIsWindows()) {
            command = shellQuote(hostAssembler());
            command += " /nologo /c /Fo " + shellQuote(objects_[i]);
            command += " " + shellQuote(temporaries_[i]);
        } else {
            command = shellQuote(hostCompiler());

            if (debug_) command += " -g";
            command += " -c " + shellQuote(temporaries_[i]);
            command += " -o " + shellQuote(objects_[i]);
        }
        if (runTool(command) != 0) {
            std::fprintf(stderr, "%s: the assembler failed - the command was:\n"
                                 "  %s\n", program_.c_str(), lastToolCommand.c_str());
            noteWindowsToolchain();
            return false;
        }
    }
    return true;
}

bool Driver::link() {
    std::string command;
    if (hostIsWindows()) {

        std::vector<std::string> objects;
        for (const std::string &t : temporaries_) {
            std::size_t dot = t.rfind('.');
            std::string obj = (dot == std::string::npos ? t : t.substr(0, dot))
                              + ".obj";
            std::string step = shellQuote(hostAssembler());
            step += " /nologo /c /Fo " + shellQuote(obj) + " " + shellQuote(t);
            if (runTool(step) != 0) {
                std::fprintf(stderr, "%s: the assembler failed - the command "
                                     "was:\n  %s\n", program_.c_str(),
                             lastToolCommand.c_str());
                noteWindowsToolchain();
                return false;
            }
            objects.push_back(obj);
            temporaryNames().push_back(obj);
        }

        command = shellQuote(hostLinker());
        command += " /nologo /subsystem:console /out:" + shellQuote(linkTo_);
        for (const std::string &o : objects) command += " " + shellQuote(o);

        command += " libcmt.lib libucrt.lib libvcruntime.lib kernel32.lib"
                   " legacy_stdio_definitions.lib";
    } else {
        command = shellQuote(hostCompiler());

        if (debug_) command += " -g";
        for (const std::string &t : temporaries_) command += " " + shellQuote(t);
        command += " -o " + shellQuote(linkTo_);

        command += " -lm";
    }

    int rc = runTool(command);
    if (rc != 0) {
        std::fprintf(stderr, "%s: the assembler or linker failed - the command "
                             "was:\n  %s\n", program_.c_str(), command.c_str());
        noteWindowsToolchain();
        return false;
    }
    return true;
}

std::string Driver::assemblyNameFor(const std::string &source) {
    std::size_t dot = source.rfind('.');
    std::size_t slash = source.find_last_of('/');
    bool hasSuffix = dot != std::string::npos &&
                     (slash == std::string::npos || dot > slash);
    return hasSuffix ? source.substr(0, dot) + ".s" : source + ".s";
}

std::string Driver::objectNameFor(const std::string &source) {
    std::size_t slash = source.find_last_of('/');
    std::string base = slash == std::string::npos ? source
                                                  : source.substr(slash + 1);
    std::size_t dot = base.rfind('.');
    return (dot == std::string::npos ? base : base.substr(0, dot)) + ".o";
}

bool Driver::parseArguments(int argc, char **argv) {
    std::vector<std::string> inputs;
    std::string output;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) {
            if (++i == argc) {
                std::fprintf(stderr, "%s: -o needs a file name\n", argv[0]);
                return false;
            }
            output = argv[i];
        } else if (std::strncmp(argv[i], "-I", 2) == 0) {
            const char *dir = argv[i][2] != '\0' ? argv[i] + 2 : nullptr;
            if (!dir) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -I needs a directory\n", argv[0]);
                    return false;
                }
                dir = argv[i];
            }
            searchPath_.push_back(dir);
        } else if (std::strncmp(argv[i], "-j", 2) == 0) {
            const char *n = argv[i][2] != '\0' ? argv[i] + 2 : nullptr;
            if (!n) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -j needs a number\n", argv[0]);
                    return false;
                }
                n = argv[i];
            }
            char *end = nullptr;
            long value = std::strtol(n, &end, 10);
            if (*n == '\0' || (end && *end != '\0') || value < 1) {
                std::fprintf(stderr,
                    "%s: -j needs a positive number of jobs, not '%s'\n", argv[0], n);
                return false;
            }
            threads_ = static_cast<unsigned>(value);
        } else if (std::strncmp(argv[i], "-masm=", 6) == 0) {
            const char *want = argv[i] + 6;
            if (std::strcmp(want, "gnu") == 0) {
                setWindowsAsmSyntax(true);
            } else if (std::strcmp(want, "masm") == 0 ||
                       std::strcmp(want, "intel") == 0) {
                setWindowsAsmSyntax(false);
            } else {
                std::fprintf(stderr,
                    "%s: -masm= takes 'masm' or 'gnu', not '%s'\n", argv[0], want);
                return false;
            }
        } else if (std::strncmp(argv[i], "-arch", 5) == 0) {
            const char *name = argv[i][5] == '=' ? argv[i] + 6 : nullptr;
            if (!name) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -arch needs a name - one of %s\n",
                                 argv[0], backendNames().c_str());
                    return false;
                }
                name = argv[i];
            }
            backend_ = findBackend(name);
            if (backend_ == nullptr) {
                std::fprintf(stderr, "%s: unknown architecture '%s' - one of %s\n",
                             argv[0], name, backendNames().c_str());
                return false;
            }
            if (!backend_->emits()) {
                std::fprintf(stderr, "%s: the %s backend is not written yet - it "
                             "knows what its types measure but has no instructions\n",
                             argv[0], backend_->name());
                return false;
            }
        } else if (std::strncmp(argv[i], "-D", 2) == 0 ||
                   std::strncmp(argv[i], "-U", 2) == 0) {
            bool undef = argv[i][1] == 'U';
            const char *text = argv[i][2] != '\0' ? argv[i] + 2 : nullptr;
            if (!text) {
                if (++i == argc) {
                    std::fprintf(stderr, "%s: -%c needs a name\n",
                                 argv[0], undef ? 'U' : 'D');
                    return false;
                }
                text = argv[i];
            }
            if (text[0] == '\0' || text[0] == '=') {
                std::fprintf(stderr, "%s: -%c needs a name before the '='\n",
                             argv[0], undef ? 'U' : 'D');
                return false;
            }
            addMacroEdit(text, undef);
        } else if (std::strcmp(argv[i], "-S") == 0) {
            assemblyOnly_ = true;
        } else if (std::strcmp(argv[i], "-c") == 0) {
            objectOnly_ = true;
        } else if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("cc1 %s\n", cc1Version());
            answered_ = true;
            return false;
        } else if (std::strcmp(argv[i], "-time") == 0) {
            timing_ = true;
        } else if (std::strcmp(argv[i], "-g") == 0) {
            debug_ = true;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
            return false;
        } else {
            inputs.push_back(argv[i]);
        }
    }

    if (CC1_INCLUDE_DIR[0] != '\0') searchPath_.push_back(CC1_INCLUDE_DIR);

    if (inputs.empty()) { usage(argv[0]); return false; }

    if (debug_ && !backend_->emitsLineTable()) {
        std::fprintf(stderr,
                     "%s: -g asks where each line of C went, and this compiler "
                     "writes no such thing for %s in the MASM spelling: MASM "
                     "carries no line table and ml64 builds none from it, and "
                     "a native Windows debugger wants CodeView rather than "
                     "DWARF. Add -masm=gnu, which does carry one, or compile "
                     "without -g.\n",
                     argv[0], backend_->name());
        return false;
    }

    if (assemblyOnly_ && objectOnly_) {
        std::fprintf(stderr, "%s: -S and -c ask for different things - -S stops "
                             "at assembly, -c goes one step further to an "
                             "object\n", argv[0]);
        return false;
    }

    if (assemblyOnly_) {
        if (!output.empty() && inputs.size() > 1) {
            std::fprintf(stderr,
                "%s: -o names a single output, but %zu inputs were given\n",
                argv[0], inputs.size());
            return false;
        }
        for (const std::string &in : inputs) {
            if (!output.empty()) jobs_.push_back(Job{ in, output });
            else if (toStdout_)  jobs_.push_back(Job{ in, "" });
            else                 jobs_.push_back(Job{ in, assemblyNameFor(in) });
        }
        return true;
    }

    if (backend_ != &defaultBackend()) {
        std::fprintf(stderr,
            "%s: cannot assemble %s code on this machine, which is %s - use -S "
            "to write the assembly and take it there\n",
            argv[0], backend_->name(), defaultBackend().name());
        return false;
    }

    if (objectOnly_ && !output.empty() && inputs.size() > 1) {
        std::fprintf(stderr,
            "%s: -o names a single object, but %zu inputs were given\n",
            argv[0], inputs.size());
        return false;
    }

    if (!objectOnly_)
        linkTo_ = !output.empty() ? output : (hostIsWindows() ? "a.exe" : "a.out");

    for (std::size_t i = 0; i < inputs.size(); i++) {
        std::string temp = temporaryName(static_cast<int>(i));
        temporaries_.push_back(temp);
        temporaryNames().push_back(temp);
        jobs_.push_back(Job{ inputs[i], temp });
        if (objectOnly_) objects_.push_back(output.empty()
                                            ? objectNameFor(inputs[i]) : output);
    }
    return true;
}

bool Driver::compile(const Job &job) {
    using Clock = std::chrono::steady_clock;
    auto ms = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    const Target &target = backend_->target();
    TypeTable types;

    auto t0 = Clock::now();
    Source src = Preprocessor(job.input, searchPath_, macrosFor()).run();
    auto t1 = Clock::now();

    std::vector<Token> tokens = Lexer(src).tokenize();
    auto t2 = Clock::now();

    Parser parser(src, std::move(tokens), types, target,
                  backend_->abi().structReturnLimit,
                  backend_->abi().aggregatesByReference,
                  backend_->abi().homogeneousFloatAggregates);
    Program program = parser.parse();
    auto t3 = Clock::now();

    bool ok = true;
    if (job.output.empty()) {
        std::unique_ptr<CodeGen> gen = backend_->codegen(std::cout);
        if (debug_) gen->setLineSource(&src, workingDirectory());
        gen->run(program);
    } else {
        std::ofstream file(job.output);
        if (!file) {
            std::fprintf(stderr, "%s: cannot write %s\n", program_.c_str(),
                         job.output.c_str());
            return false;
        }
        std::unique_ptr<CodeGen> gen = backend_->codegen(file);
        if (debug_) gen->setLineSource(&src, workingDirectory());
        gen->run(program);
    }
    auto t4 = Clock::now();

    if (timing_) {
        double read = ms(t0, t1), lex = ms(t1, t2), parse = ms(t2, t3), gen = ms(t3, t4);
        double all = ms(t0, t4);
        std::fprintf(stderr,
            "%s: read+pp %.2f  lex %.2f  parse %.2f  codegen %.2f  total %.2f ms"
            "   (front end %.0f%%)\n",
            job.input.c_str(), read, lex, parse, gen, all,
            all > 0 ? 100.0 * (read + lex + parse) / all : 0.0);
    }
    return ok;
}

unsigned Driver::availableCores() {
#ifdef __linux__
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof allowed, &allowed) == 0) {
        std::vector<std::pair<long, long>> cores;
        for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
            if (!CPU_ISSET(cpu, &allowed)) continue;
            std::string base = "/sys/devices/system/cpu/cpu" +
                               std::to_string(cpu) + "/topology/";
            std::ifstream pkgFile(base + "physical_package_id");
            std::ifstream coreFile(base + "core_id");
            long pkg = 0, core = cpu;
            if (!(pkgFile >> pkg) || !(coreFile >> core)) { pkg = 0; core = cpu; }

            std::pair<long, long> id(pkg, core);
            bool seen = false;
            for (const std::pair<long, long> &k : cores)
                if (k == id) { seen = true; break; }
            if (!seen) cores.push_back(id);
        }
        if (!cores.empty()) return static_cast<unsigned>(cores.size());
    }
#endif
    unsigned n = std::thread::hardware_concurrency();
    return n != 0 ? n : 1;
}

unsigned Driver::threadCount() const {
    if (threads_ == 1) return 1;

    unsigned want;
    if (threads_ != 0) {
        want = threads_;
    } else {
        if (jobs_.size() < kThreadFrom) return 1;
        want = availableCores();
    }
    if (want > jobs_.size()) want = static_cast<unsigned>(jobs_.size());
    return want < 1 ? 1 : want;
}

bool Driver::runJobs() {
    unsigned n = threadCount();

    if (timing_)
        std::fprintf(stderr, "%s: %zu jobs on %u thread%s\n", program_.c_str(),
                     jobs_.size(), n, n == 1 ? "" : "s");

    if (n <= 1) {
        for (const Job &job : jobs_)
            if (!compile(job)) return false;
        return true;
    }

    std::atomic<std::size_t> next{0};
    std::atomic<bool> ok{true};

    std::vector<std::thread> pool;
    pool.reserve(n);
    for (unsigned t = 0; t < n; t++) {
        pool.emplace_back([this, &next, &ok] {
            for (;;) {
                std::size_t i = next.fetch_add(1);
                if (i >= jobs_.size()) return;
                if (!compile(jobs_[i])) { ok.store(false); return; }
            }
        });
    }
    for (std::thread &t : pool) t.join();
    return ok.load();
}

int Driver::run(int argc, char **argv) {
    program_ = argv[0];

    int inputs = 0;
    bool sawO = false, sawS = false;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-o") == 0) { sawO = true; i++; }
        else if (std::strcmp(argv[i], "-I") == 0) i++;
        else if (std::strcmp(argv[i], "-j") == 0) i++;
        else if (std::strcmp(argv[i], "-arch") == 0) i++;
        else if (std::strcmp(argv[i], "-D") == 0) i++;
        else if (std::strcmp(argv[i], "-U") == 0) i++;
        else if (std::strcmp(argv[i], "-S") == 0) sawS = true;
        else if (argv[i][0] != '-') inputs++;
    }
    toStdout_ = (sawS && inputs == 1 && !sawO);

    // **A question answered is not a failure.** --version is a request this
    // program can satisfy, so it leaves with 0; a bad argument leaves with 1.
    // Both used to be 1, which is why a script asking three compilers their
    // versions stopped at the first one.
    if (!parseArguments(argc, argv)) return answered_ ? 0 : 1;

    std::atexit([] {
        std::vector<std::string> &names = temporaryNames();
        for (const std::string &t : names) std::remove(t.c_str());
        names.clear();
    });

    if (!runJobs()) { removeTemporaries(); return 1; }
    if (assemblyOnly_) return 0;

    bool ok = objectOnly_ ? assembleObjects() : link();
    removeTemporaries();
    return ok ? 0 : 1;
}
