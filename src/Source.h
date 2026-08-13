// Source.h - the text being compiled, and the only place that reports on it.
//
// Every stage is handed the Source rather than a bare string, so an error found
// in code generation can still point at the column it came from. Diagnostics
// live here because the offset is only meaningful next to the text it indexes.
#pragma once

#include <string>
#include <vector>

class Source {
public:
    // Where one line of the text came from. The preprocessor supplies these so
    // that a message about a line spliced in by #include names the file it was
    // written in rather than the position it ended up at. Without them - which
    // is every file that uses no directives - nothing here behaves differently.
    struct Line {
        int file;
        int line;
    };

    Source(std::string name, std::string text);
    Source(std::string name, std::string text, std::vector<std::string> files,
           std::vector<Line> lines);

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
    std::vector<std::string> files_;
    std::vector<Line> lines_;
};
