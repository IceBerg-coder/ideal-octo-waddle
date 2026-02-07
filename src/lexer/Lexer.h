#ifndef PYNEXT_LEXER_H
#define PYNEXT_LEXER_H

#include "Token.h"
#include <string_view>

namespace pynext {

class Lexer {
public:
    explicit Lexer(std::string_view source);

    Token nextToken();
    char peek() const;

private:
    void advance();
    void skipWhitespace();
    Token atom(TokenKind kind);
    Token identifier();
    Token number();
    Token string();

    std::string_view source;
    int position = 0;
    int line = 1;
    int column = 1;
    int startPosition = 0; // Start of the current token
};

} // namespace pynext

#endif // PYNEXT_LEXER_H
