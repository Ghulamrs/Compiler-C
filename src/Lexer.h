#pragma once

#include <string>
#include <vector>

class Source;

enum class TokenKind { Punct, Num, Str, Ident, Keyword, End };

struct Token {
    TokenKind kind = TokenKind::End;
    long value = 0;
    bool suffixU = false;
    bool suffixL = false;
    bool isFloat = false;
    bool suffixF = false;
    double dvalue = 0;
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
