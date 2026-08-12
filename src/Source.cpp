#include "Source.h"

#include <cstdio>
#include <cstdlib>

Source::Source(std::string name, std::string text)
    : name_(std::move(name)), text_(std::move(text)) {
    // The diagnostic printer looks for a newline to bound the offending line.
    // A file not ending in one is legal C and awkward here, so supply it.
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

    int indent = std::fprintf(stderr, "%s:%d: ", name_.c_str(), lineNo);
    std::fprintf(stderr, "%.*s\n", static_cast<int>(lineEnd - lineStart),
                 text_.c_str() + lineStart);
    std::fprintf(stderr, "%*s^ %s\n",
                 indent + static_cast<int>(pos - lineStart), "", message.c_str());
    std::exit(1);
}
