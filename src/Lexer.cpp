#include "Lexer.h"
#include "Source.h"

#include <cctype>
#include <cstdlib>

bool Lexer::isKeyword(const std::string &word) {
    // Checked after an identifier is scanned, never as a prefix: "returned"
    // and "integer" are identifiers, and a prefix test takes the front off both.
    static const char *const kw[] = {
        "int", "return", "void", "if", "else", "while",
        "char", "short", "long", "signed", "unsigned", "sizeof",
        "static", "extern", "const", "register"
    };
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

    // Longest match first, always. Scanning "<" before "<=" would split the
    // operator in two and leave a stray "=" that parses as an assignment.
    static const char *const two[] = { "==", "!=", "<=", ">=", "<<", ">>", "&&", "||" };

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
            // Base 0, so 0x1f and 017 are read the way C reads them.
            t.value = std::strtol(s.c_str() + i, &stop, 0);
            i = static_cast<std::size_t>(stop - s.c_str());
            // A suffix is part of the constant and decides its type: 1u is
            // unsigned, 1l is long. Either order, either case.
            for (int n = 0; n < 2 && i < s.size(); n++) {
                if (s[i] == 'u' || s[i] == 'U')      { t.suffixU = true; i++; }
                else if (s[i] == 'l' || s[i] == 'L') { t.suffixL = true; i++; }
                else break;
            }
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

        bool matched = false;
        for (const char *op : two) {
            if (s.compare(i, 2, op) == 0) {
                Token t;
                t.kind = TokenKind::Punct;
                t.text = op;
                t.pos = i;
                out.push_back(std::move(t));
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched) continue;

        if (std::string("+-*/%()<>={},;!").find(c) != std::string::npos) {
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
