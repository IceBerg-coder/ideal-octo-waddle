#ifndef PYNEXT_PARSER_H
#define PYNEXT_PARSER_H

#include "../lexer/Lexer.h"
#include "AST.h"
#include <vector>
#include <memory>

namespace pynext {

class Parser {
public:
    explicit Parser(Lexer& lexer) : lexer(lexer) {
        // Prime the first token
        advance();
    }

    std::vector<std::unique_ptr<Stmt>> parseModule();

private:
    Lexer& lexer;
    Token currentToken;

    void advance();
    bool match(TokenKind kind);
    Token consume(TokenKind kind, const std::string& errorMsg);

    // Initial simple grammar
    // Module -> (FunctionDecl | Statement)*
    // Statement -> ReturnStmt | IfStmt | ExprStmt
    // Expr -> Binary equality comparison ...
    
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<FunctionStmt> parseFunction();
    std::unique_ptr<StructDeclStmt> parseStruct();
    std::unique_ptr<FunctionStmt> parseExtern();
    std::unique_ptr<Block> parseBlock();
    
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parsePrimary();
    std::unique_ptr<Expr> parseBinary(int precedence, std::unique_ptr<Expr> lhs);
    
    std::string parseTypeName();
    
    int getPrecedence(TokenKind kind);
};

} // namespace pynext

#endif // PYNEXT_PARSER_H
