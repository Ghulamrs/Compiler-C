#pragma once

#include <string>
#include <vector>

class Source {
public:
    struct Line {
        int file;
        int line;
    };

    Source(std::string name, std::string text);
    Source(std::string name, std::string text, std::vector<std::string> files,
           std::vector<Line> lines);

    const std::string &text() const { return text_; }
    const char *begin() const { return text_.c_str(); }

    // Where a byte offset falls, once the preprocessor's #line records have
    // been honoured: a file, the line a user would name, and the column.
    // Diagnostics have always worked this out; a line table needs the same
    // answer for every statement, which is why it is a method now.
    struct Place {
        int file;       // an index into files()
        int line;
        int column;
    };
    Place locate(std::size_t pos) const;

    // The file table, [0] being this translation unit's own name. Never empty,
    // so an index out of a Place always names something.
    const std::vector<std::string> &files() const { return files_; }

    static Source fromFile(const std::string &path);

    [[noreturn]] void fail(std::size_t pos, const std::string &message) const;

private:
    std::string name_;
    std::string text_;
    std::vector<std::string> files_;
    std::vector<Line> lines_;

    // Where each line begins, so locating an offset is a search rather than a
    // count. Built on first use and not before: a compile that reports no
    // diagnostic and is asked for no line table never needs it, and building
    // it eagerly would put another pass over the text in front of every job.
    mutable std::vector<std::size_t> lineStarts_;
    void indexLines() const;
    // The 1-based line an offset is on, and where that line starts.
    void lineAt(std::size_t pos, int *line, std::size_t *start) const;
};
