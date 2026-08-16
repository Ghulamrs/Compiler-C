#pragma once

#include <string>
#include <vector>

class Source;

enum class TokenKind { Punct, Num, Str, Ident, Keyword, End };

struct Token {
    TokenKind kind = TokenKind::End;
    long long value = 0;
    bool suffixU = false;
    bool suffixL = false;
    bool suffixLL = false;
    bool isFloat = false;
    bool suffixF = false;
    // Held at the widest floating type the *host* has, because a target's
    // 'long double' may be wider than its double and a constant read into a
    // double could not then be written back out. On the box this compiler is
    // built on that is x87's 80-bit format, which is exactly what the Linux
    // target wants. It is also the one place cross-compilation is lossy: a cc1
    // built where long double is double - Apple's arm64, the UCRT - cannot
    // hold an x86_64-linux long double constant to its full width. Named in
    // docs/STATUS.md rather than left to be discovered.
    long double dvalue = 0;
    // An 'L' prefix. L'x' and L"..." are made of wchar_t rather than char, and
    // how wide that is belongs to the target rather than to the lexer - so the
    // token records only that the prefix was there.
    bool wide = false;
    std::string text;
    std::size_t pos = 0;

    bool is(const char *s) const { return text == s; }
};

class Lexer {
public:
    explicit Lexer(const Source &src) : src_(src) {}

    std::vector<Token> tokenize();

private:
    const Source &src_;

    static bool isKeyword(const std::string &word);
};
