// Source.h - the text being compiled, and the only place that reports on it.
//
// Every stage is handed the Source rather than a bare string, so an error found
// in code generation can still point at the column it came from. Diagnostics
// live here because the offset is only meaningful next to the text it indexes.
#pragma once

#include <string>

class Source {
public:
    Source(std::string name, std::string text);

    const std::string &text() const { return text_; }
    const char *begin() const { return text_.c_str(); }

    // Reads the file, or exits saying why. A constructor cannot fail usefully
    // in a program with no exception policy, so opening is a separate step.
    static Source fromFile(const std::string &path);

    // Prints the offending line with a caret under it, then exits. Nothing
    // recovers from a syntax error yet, and pretending otherwise would mean
    // every caller checking a return it cannot act on.
    [[noreturn]] void fail(std::size_t pos, const std::string &message) const;

private:
    std::string name_;
    std::string text_;
};
