#include "Lexer.h"
#include "Source.h"

#include <cctype>
#include <cstdlib>

bool Lexer::isKeyword(const std::string &word) {
    // Checked after an identifier is scanned, never as a prefix: "returned"
    // and "integer" are identifiers, and a prefix test takes the front off both.
    static const char *const kw[] = { "int", "return", "void" };
    for (const char *k : kw)
        if (word == k) return true;
    return false;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    const std::string &s = src_.text();
    std::size_t i = 0;

    auto identStart = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; };
    auto identCont  = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };

    while (i < s.size()) {
        char c = s[i];

        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        // A comment is whitespace that happens to be long.
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
            while (i < s.size() && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
            std::size_t end = s.find("*/", i + 2);
            if (end == std::string::npos) src_.fail(i, "unterminated comment");
            i = end + 2;
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            Token t;
            t.kind = TokenKind::Num;
            t.pos = i;
            char *stop = nullptr;
            t.value = std::strtol(s.c_str() + i, &stop, 10);
            i = static_cast<std::size_t>(stop - s.c_str());
            out.push_back(std::move(t));
            continue;
        }

        if (identStart(c)) {
            std::size_t start = i;
            while (i < s.size() && identCont(s[i])) i++;
            Token t;
            t.text = s.substr(start, i - start);
            t.kind = isKeyword(t.text) ? TokenKind::Keyword : TokenKind::Ident;
            t.pos = start;
            out.push_back(std::move(t));
            continue;
        }

        if (std::string("+-*/(){};").find(c) != std::string::npos) {
            Token t;
            t.kind = TokenKind::Punct;
            t.text.assign(1, c);
            t.pos = i;
            out.push_back(std::move(t));
            i++;
            continue;
        }

        src_.fail(i, std::string("stray '") + c + "' in program");
    }

    Token end;
    end.kind = TokenKind::End;
    end.pos = s.size();
    out.push_back(std::move(end));
    return out;
}
