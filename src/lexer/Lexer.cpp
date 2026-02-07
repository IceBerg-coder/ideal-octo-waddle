#include "Lexer.h"
#include <cctype>
#include <unordered_map>

namespace pynext {

Lexer::Lexer(std::string_view source) : source(source) {}

char Lexer::peek() const {
    if (position >= source.length()) return '\0';
    return source[position];
}

void Lexer::advance() {
    if (position < source.length()) {
        if (source[position] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        position++;
    }
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        if (std::isspace(c)) {
            advance();
        } else if (c == '#') { // Comment support
            while (peek() != '\n' && peek() != '\0') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::atom(TokenKind kind) {
    std::string_view text = source.substr(startPosition, position - startPosition);
    return Token{kind, text, line, column - (int)text.length()};
}

Token Lexer::identifier() {
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string_view text = source.substr(startPosition, position - startPosition);
    
    // Simple keyword map
    if (text == "def") return atom(TokenKind::Def);
    if (text == "end") return atom(TokenKind::End);
    if (text == "if") return atom(TokenKind::If);
    if (text == "else") return atom(TokenKind::Else);
    if (text == "return") return atom(TokenKind::Return);
    if (text == "var") return atom(TokenKind::Var);
    if (text == "struct") return atom(TokenKind::Struct);
    if (text == "extern") return atom(TokenKind::Extern);
    if (text == "while") return atom(TokenKind::While);
    if (text == "true") return atom(TokenKind::True);
    if (text == "false") return atom(TokenKind::False);

    return atom(TokenKind::Identifier);
}

Token Lexer::number() {
    bool isFloat = false;
    while (std::isdigit(peek())) {
        advance();
    }
    
    if (peek() == '.') {
        isFloat = true;
        advance(); // consume dot
        while (std::isdigit(peek())) {
            advance();
        }
    }
    
    return atom(isFloat ? TokenKind::Float : TokenKind::Integer);
}

Token Lexer::string() {
    advance(); // Skip opening "
    while (peek() != '"' && peek() != '\0') {
        advance();
    }
    if (peek() == '"') advance(); // Skip closing "
    
    size_t fullLen = position - startPosition;
    std::string_view text = source.substr(startPosition + 1, fullLen - 2);
    return Token{TokenKind::String, text, line, column - (int)fullLen};
}


Token Lexer::nextToken() {
    skipWhitespace();
    startPosition = position;

    char c = peek();
    if (c == '\0') return atom(TokenKind::EndOfFile);

    if (std::isalpha(c) || c == '_') {
        advance();
        return identifier();
    }

    if (std::isdigit(c)) {
        return number();
    }
    
    if (c == '"') {
        return string();
    }

    advance(); // Consume the char for the single-char tokens

    switch (c) {
        case '+': return atom(TokenKind::Plus);
        case '*': return atom(TokenKind::Star);
        case '/': return atom(TokenKind::Slash);
        case '(': return atom(TokenKind::LParen);
        case ')': return atom(TokenKind::RParen);
        case ',': return atom(TokenKind::Comma);
        case ':': return atom(TokenKind::Colon);
        case '.': return atom(TokenKind::Dot);
        case '<': return atom(TokenKind::LessThan);
        case '>': return atom(TokenKind::GreaterThan);
        case '-':
            if (peek() == '>') {
                advance();
                return atom(TokenKind::Arrow);
            }
            return atom(TokenKind::Minus);
        case '=':
            if (peek() == '=') {
                advance();
                return atom(TokenKind::EqualEqual);
            }
            return atom(TokenKind::Equal);
        case '!':
            if (peek() == '=') {
                advance();
                return atom(TokenKind::NotEqual);
            }
            return atom(TokenKind::Error);
    }

    return atom(TokenKind::Error);
}

} // namespace pynext
