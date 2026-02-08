#ifndef PYNEXT_TOKEN_H
#define PYNEXT_TOKEN_H

#include <string>
#include <string_view>
#include <iostream>

namespace pynext {

enum class TokenKind {
    // End of file
    EndOfFile,

    // Error
    Error,

    // Identifiers & Literals
    Identifier,
    Integer,
    Float,
    String,

    // Keywords
    Def,
    End,
    If,
    Else,
    Return,
    Var,
    Struct,
    Extern,
    While,
    True,
    False,
    
    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Equal,          // =
    EqualEqual,     // ==
    NotEqual,       // !=
    Arrow,          // ->
    
    // Punctuation
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    Comma,          // ,
    Colon,          // :
    Dot,            // .
    LessThan,       // <
    GreaterThan,    // >
};

struct Token {
    TokenKind kind;
    std::string_view text;
    int line;
    int column;

    // Helper for debugging/printing
    std::string check() const {
        return std::string(text);
    }
};

inline std::string toString(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile: return "EndOfFile";
        case TokenKind::Error: return "Error";
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::Integer: return "Integer";
        case TokenKind::Float: return "Float";
        case TokenKind::String: return "String";
        case TokenKind::Def: return "Def";
        case TokenKind::End: return "End";
        case TokenKind::If: return "If";
        case TokenKind::Else: return "Else";
        case TokenKind::Return: return "Return";
        case TokenKind::Var: return "Var";
        case TokenKind::Struct: return "Struct";
        case TokenKind::Extern: return "Extern";
        case TokenKind::While: return "While";
        case TokenKind::True: return "True";
        case TokenKind::False: return "False";
        case TokenKind::Plus: return "Plus";
        case TokenKind::Minus: return "Minus";
        case TokenKind::Star: return "Star";
        case TokenKind::Slash: return "Slash";
        case TokenKind::Equal: return "Equal";
        case TokenKind::EqualEqual: return "EqualEqual";
        case TokenKind::NotEqual: return "NotEqual";
        case TokenKind::Arrow: return "Arrow";
        case TokenKind::LParen: return "LParen";
        case TokenKind::RParen: return "RParen";
        case TokenKind::Comma: return "Comma";
        case TokenKind::Colon: return "Colon";
        case TokenKind::Dot: return "Dot";
        case TokenKind::LessThan: return "LessThan";
        case TokenKind::GreaterThan: return "GreaterThan";
        default: return "Unknown";
    }
}

} // namespace pynext

#endif // PYNEXT_TOKEN_H
