// Preprocessor.h - the stage before the lexer.
//
// It takes a file and produces one translation unit: includes spliced in,
// conditionals resolved, macros expanded. What comes out is text, and the lexer
// that reads it does not know this stage exists.
//
// Text out, rather than tokens out, for one reason that is worth stating
// because it decided the design. Every diagnostic in this compiler is a byte
// offset into a Source, and there are three hundred tests that depend on the
// exact output of Source::fail. Emitting text means a file containing no
// directives comes through byte for byte unchanged - so nothing about the
// existing diagnostics moves, and only files that actually use the
// preprocessor can be affected by it.
//
// The cost is that offsets no longer point where the reader wrote the code once
// an include has been spliced in. That is paid for with a line map: one entry
// per emitted line saying which file and which line it came from, which Source
// consults so a message about an included file names that file.
//
// Substitution is not done on raw text. The scanner here knows string literals,
// character constants and both kinds of comment, so a macro named "n" does not
// rewrite the middle of "an error" or of a comment. That is the one part of a
// text-level preprocessor that has to be token-aware to be correct at all.
#pragma once

#include "Source.h"

#include <string>
#include <unordered_map>
#include <vector>

class Preprocessor {
public:
    explicit Preprocessor(std::string path) : path_(std::move(path)) {}

    // The expanded translation unit, ready for the lexer. Exits with a message
    // if a directive is malformed or a file cannot be opened.
    Source run();

private:
    struct Macro {
        std::string body;
        // Where it was defined, so a message about it can point somewhere real.
        int file = 0;
        int line = 0;
    };

    // One of these per #if-family directive, innermost last. "taken" says
    // whether any arm of this conditional has already been used, which is what
    // #else needs to know.
    struct Cond {
        bool active;
        bool taken;
        bool seenElse;
    };

    std::string path_;
    std::unordered_map<std::string, Macro> macros_;

    std::string out_;
    std::vector<std::string> files_;
    std::vector<Source::Line> lines_;   // one entry per line of out_

    std::vector<Cond> conds_;
    bool inBlockComment_ = false;
    int depth_ = 0;

    // Whether the innermost conditional lets text through. Skipped text is
    // still scanned for directives, because #endif has to be found.
    bool emitting() const;

    void processFile(const std::string &path, int fileIndex);
    void directive(const std::string &line, int fileIndex, int lineNo);
    void emitLine(const std::string &text, int fileIndex, int lineNo);

    // Macro expansion over one line, leaving literals and comments alone.
    std::string expandLine(const std::string &line, int fileIndex, int lineNo);
    // One name. busy holds the macros already being expanded above this point,
    // which is what stops "#define N N" from expanding for ever - C says a
    // macro is not replaced inside its own expansion.
    std::string expandName(const std::string &name, std::vector<std::string> &busy,
                           int fileIndex, int lineNo);

    // A #if or #elif condition. Evaluated in a world where no type exists yet:
    // "defined X" is resolved first, then macros are expanded, then any name
    // still standing is 0 - which is C's rule and the reason "#if FOO" works on
    // a FOO that was never defined.
    long evalCondition(const std::string &expr, int fileIndex, int lineNo,
                       const std::string &line);
    // Replaces "defined X" and "defined(X)" with 1 or 0. Done before expansion,
    // because the operand of defined is a name and not something to expand.
    std::string resolveDefined(const std::string &expr, int fileIndex, int lineNo,
                               const std::string &line);
    // Whether everything outside the innermost conditional is letting text
    // through. #elif needs this: its own arm can only be taken if the
    // conditional it belongs to is itself inside live text.
    bool parentEmitting() const;

    // The same shape of message Source::fail prints, from a stage that has no
    // Source yet.
    [[noreturn]] void fail(int fileIndex, int lineNo, const std::string &line,
                           std::size_t column, const std::string &message) const;

    // The directory of a file, for resolving an include beside it.
    static std::string directoryOf(const std::string &path);
};
