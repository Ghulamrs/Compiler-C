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

    std::string head = std::string(file) + ":" + std::to_string(reported) + ": ";
    std::string text = head;
    text.append(text_, lineStart, lineEnd - lineStart);
    text += "\n";
    text.append(head.size() + (pos - lineStart), ' ');
    text += "^ " + message + "\n";
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::exit(1);
}
