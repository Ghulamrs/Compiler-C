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
    // Held at the widest floating type the *host* has, because a target's long
    // double may be wider than the host's double.
    long double dvalue = 0;
    // L'x' and L"..." are made of wchar_t rather than char.
    bool wide = false;
    std::string text;
    std::size_t pos = 0;

    // Only a punctuator, an identifier and a keyword have a spelling; a
    // string's text is its contents. Without the kind test the literal "*"
    // answered to is("*") and the parser read it as an operator.
    bool is(const char *s) const {
        return (kind == TokenKind::Punct || kind == TokenKind::Ident ||
                kind == TokenKind::Keyword) && text == s;
    }
};

class Lexer {
public:
    explicit Lexer(const Source &src) : src_(src) {}

    std::vector<Token> tokenize();

private:
    const Source &src_;

    static bool isKeyword(const std::string &word);
};
