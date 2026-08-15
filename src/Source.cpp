#include "Source.h"

#include <cstdio>
#include <cstdlib>

Source::Source(std::string name, std::string text)
    : name_(std::move(name)), text_(std::move(text)) {
    if (text_.empty() || text_.back() != '\n') text_.push_back('\n');
}

Source::Source(std::string name, std::string text, std::vector<std::string> files,
               std::vector<Line> lines)
    : name_(std::move(name)), text_(std::move(text)),
      files_(std::move(files)), lines_(std::move(lines)) {
    if (text_.empty() || text_.back() != '\n') text_.push_back('\n');
}

Source Source::fromFile(const std::string &path) {
    std::FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        std::exit(1);
    }
    std::string buf;
    char chunk[4096];
    std::size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, fp)) > 0) buf.append(chunk, n);
    std::fclose(fp);
    return Source(path, std::move(buf));
}

void Source::fail(std::size_t pos, const std::string &message) const {
    if (pos > text_.size()) pos = text_.size();

    std::size_t lineStart = text_.rfind('\n', pos == 0 ? 0 : pos - 1);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;

    std::size_t lineEnd = text_.find('\n', pos);
    if (lineEnd == std::string::npos) lineEnd = text_.size();

    int lineNo = 1;
    for (std::size_t i = 0; i < lineStart; i++)
        if (text_[i] == '\n') lineNo++;

    const char *file = name_.c_str();
    int reported = lineNo;
    if (!lines_.empty() && lineNo >= 1 &&
        static_cast<std::size_t>(lineNo) <= lines_.size()) {
        const Line &l = lines_[static_cast<std::size_t>(lineNo) - 1];
        if (l.file >= 0 && static_cast<std::size_t>(l.file) < files_.size())
            file = files_[static_cast<std::size_t>(l.file)].c_str();
        reported = l.line;
    }

    // The first line is the one every other tool reads. 'file:line:col: error: '
    // is what gcc and clang emit and what an IDE's log parser matches on, so it
    // is what puts a cc1 refusal in Xcode's gutter and its issue list instead of
    // only in the raw build log. Until this was here the message sat on the line
    // *after* a caret, under a 'file:line:' with no column and no severity, and
    // nothing but a human reading the log ever saw it.
    //
    // The caret display below is for that human, and repeats nothing the parser
    // needs.
    std::string text = std::string(file) + ":" + std::to_string(reported) + ":" +
                       std::to_string(pos - lineStart + 1) + ": error: " +
                       message + "\n";

    text += "    ";
    text.append(text_, lineStart, lineEnd - lineStart);
    text += "\n    ";
    // Pad with the line's own whitespace, so the caret stays under its column
    // when the source is indented with tabs. Anything else becomes a space.
    for (std::size_t i = lineStart; i < pos; i++)
        text += (text_[i] == '\t') ? '\t' : ' ';
    text += "^\n";

    std::fwrite(text.data(), 1, text.size(), stderr);
    std::exit(1);
}
