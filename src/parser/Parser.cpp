#include "Parser.h"

namespace pynext {

void Parser::advance() {
    currentToken = lexer.nextToken();
}

bool Parser::match(TokenKind kind) {
    if (currentToken.kind == kind) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenKind kind, const std::string& errorMsg) {
    if (currentToken.kind == kind) {
        Token token = currentToken;
        advance();
        return token;
    }
    std::cerr << "Parser Error: " << errorMsg << " Got: " << toString(currentToken.kind) << "\n";
    // For now, return error token or throw (simple recovery approach: exit)
    exit(1); 
}

std::vector<std::unique_ptr<Stmt>> Parser::parseModule() {
    std::vector<std::unique_ptr<Stmt>> statements;
    while (currentToken.kind != TokenKind::EndOfFile) {
        if (currentToken.kind == TokenKind::Def) {
            statements.push_back(parseFunction());
        } else if (currentToken.kind == TokenKind::Struct) {
            statements.push_back(parseStruct());
        } else if (currentToken.kind == TokenKind::Extern) {
            statements.push_back(parseExtern());
        } else {
            statements.push_back(parseStatement());
        }
    }
    return statements;
}

std::unique_ptr<StructDeclStmt> Parser::parseStruct() {
    consume(TokenKind::Struct, "Expected 'struct'");
    std::string name = std::string(consume(TokenKind::Identifier, "Expected struct name").text);
    
    std::vector<std::pair<std::string, std::string>> fields;
    while (currentToken.kind != TokenKind::End && currentToken.kind != TokenKind::EndOfFile) {
        std::string fieldName = std::string(consume(TokenKind::Identifier, "Expected field name").text);
        consume(TokenKind::Colon, "Expected ':'");
        std::string typeName = parseTypeName();
        fields.push_back({fieldName, typeName});
    }
    consume(TokenKind::End, "Expected 'end' after struct body");
    
    return std::make_unique<StructDeclStmt>(name, fields);
}

std::unique_ptr<FunctionStmt> Parser::parseExtern() {
    consume(TokenKind::Extern, "Expected 'extern'");
    consume(TokenKind::Def, "Expected 'def' after 'extern'");
    std::string name = std::string(consume(TokenKind::Identifier, "Expected function name").text);

    consume(TokenKind::LParen, "Expected '('");
    std::vector<std::pair<std::string, std::string>> params;
    if (currentToken.kind != TokenKind::RParen) {
        do {
            std::string paramName = std::string(consume(TokenKind::Identifier, "Expected parameter name").text);
            consume(TokenKind::Colon, "Expected ':' for type");
            std::string typeName = parseTypeName();
            params.push_back({paramName, typeName});
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "Expected ')'");

    std::string returnType = "void";
    if (match(TokenKind::Arrow)) {
        returnType = parseTypeName();
    }
    
    // Externs have no body
    return std::make_unique<FunctionStmt>(name, params, returnType, nullptr);
}

std::unique_ptr<FunctionStmt> Parser::parseFunction() {
    consume(TokenKind::Def, "Expected 'def'");
    std::string name = std::string(consume(TokenKind::Identifier, "Expected function name").text);
    
    consume(TokenKind::LParen, "Expected '('");
    std::vector<std::pair<std::string, std::string>> params;
    if (currentToken.kind != TokenKind::RParen) {
        do {
            std::string paramName = std::string(consume(TokenKind::Identifier, "Expected parameter name").text);
            consume(TokenKind::Colon, "Expected ':' for type");
            std::string typeName = parseTypeName();
            params.push_back({paramName, typeName});
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "Expected ')'");
    
    std::string returnType = "void";
    if (match(TokenKind::Arrow)) {
        returnType = parseTypeName();
    }
    
    auto body = parseBlock();
    consume(TokenKind::End, "Expected 'end' after function body");
    
    return std::make_unique<FunctionStmt>(name, params, returnType, std::move(body));
}

std::unique_ptr<Block> Parser::parseBlock() {
    auto block = std::make_unique<Block>();
    while (currentToken.kind != TokenKind::End && 
           currentToken.kind != TokenKind::Else && 
           currentToken.kind != TokenKind::EndOfFile) {
        block->statements.push_back(parseStatement());
    }
    return block;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (match(TokenKind::Return)) {
        if (currentToken.kind == TokenKind::End || currentToken.kind == TokenKind::EndOfFile || 
            currentToken.kind == TokenKind::Else) {
             // Empty return ? technically valid in void functions
             // But for now let's assume valid return is expression
             return std::make_unique<ReturnStmt>(nullptr); 
        }
        // Hack: Check if next token starts an expression
        // We really need a better check here, but let's assume it does.
        auto expr = parseExpression();
        return std::make_unique<ReturnStmt>(std::move(expr));
    }
    
    if (match(TokenKind::If)) {
        auto condition = parseExpression();
        auto thenBranch = parseBlock();
        std::unique_ptr<Block> elseBranch = nullptr;
        
        if (match(TokenKind::Else)) {
            elseBranch = parseBlock();
        }
        consume(TokenKind::End, "Expected 'end' after if");
        return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
    }

    if (match(TokenKind::While)) {
        auto condition = parseExpression();
        auto body = parseBlock();
        consume(TokenKind::End, "Expected 'end' after while");
        return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
    }
    
    if (match(TokenKind::Var)) {
        std::string name = std::string(consume(TokenKind::Identifier, "Expected variable name").text);
        std::string typeName = "";
        
        if (match(TokenKind::Colon)) {
            typeName = parseTypeName();
        }
        
        std::unique_ptr<Expr> init = nullptr;
        if (match(TokenKind::Equal)) {
            init = parseExpression();
        } else if (typeName.empty()) {
             // Error if no type and no initializer
             consume(TokenKind::Equal, "Expected '=' or type annotation for 'var'");
        }
        
        return std::make_unique<VarDeclStmt>(name, typeName, std::move(init));
    }

    // Fallback: Expression statement
    auto expr = parseExpression();
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<Expr> Parser::parseExpression() {
    auto lhs = parsePrimary();
    return parseBinary(0, std::move(lhs));
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    std::unique_ptr<Expr> lhs;
    if (currentToken.kind == TokenKind::Identifier) {
        std::string name = std::string(currentToken.text);
        advance();
        if (match(TokenKind::LParen)) {
            // ... CallExpr logic
            std::vector<std::unique_ptr<Expr>> args;
            if (currentToken.kind != TokenKind::RParen) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RParen, "Expected ')'");
            lhs = std::make_unique<CallExpr>(name, std::move(args));
        } else {
            lhs = std::make_unique<VariableExpr>(name);
        }
    } else if (currentToken.kind == TokenKind::Integer) {
        std::string val = std::string(currentToken.text);
        advance();
        lhs = std::make_unique<LiteralExpr>(val, false);
    } else if (currentToken.kind == TokenKind::Float) {
         // ...
         std::string val = std::string(currentToken.text);
         advance();
         lhs = std::make_unique<LiteralExpr>(val, true);
    } else if (currentToken.kind == TokenKind::True) {
        advance();
        lhs = std::make_unique<LiteralExpr>("true", false, true);
    } else if (currentToken.kind == TokenKind::False) {
        advance();
        lhs = std::make_unique<LiteralExpr>("false", false, true);
    } else if (currentToken.kind == TokenKind::String) {
        std::string val = std::string(currentToken.text);
        advance();
        lhs = std::make_unique<LiteralExpr>(val, false, false, true);
    } else if (match(TokenKind::LBracket)) {
        std::vector<std::unique_ptr<Expr>> elements;
        if (currentToken.kind != TokenKind::RBracket) {
            do {
                elements.push_back(parseExpression());
            } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RBracket, "Expected ']'");
        lhs = std::make_unique<ArrayLiteralExpr>(std::move(elements));
    } else if (match(TokenKind::LParen)) {
        lhs = parseExpression();
        consume(TokenKind::RParen, "Expected ')'");
    } else {
        std::cerr << "Unexpected token in expression: " << toString(currentToken.kind) << "\n";
        exit(1);
    }
    
    // Handle Postfix Expressions (Member Access, Indexing)
    while (true) {
        if (match(TokenKind::Dot)) {
            std::string member = std::string(consume(TokenKind::Identifier, "Expected member name after '.'").text);
            lhs = std::make_unique<MemberAccessExpr>(std::move(lhs), member);
        } else if (match(TokenKind::LBracket)) {
            auto index = parseExpression();
            consume(TokenKind::RBracket, "Expected ']' after index");
            lhs = std::make_unique<IndexExpr>(std::move(lhs), std::move(index));
        } else {
            break;
        }
    }
    
    return lhs;
}

int Parser::getPrecedence(TokenKind kind) {
    switch (kind) {
        case TokenKind::Star:
        case TokenKind::Slash: return 5;
        case TokenKind::Plus:
        case TokenKind::Minus: return 4;
        case TokenKind::LessThan:
        case TokenKind::GreaterThan: return 3;
        case TokenKind::EqualEqual: 
        case TokenKind::NotEqual: return 2;
        case TokenKind::Equal: return 1;
        default: return -1;
    }
}

std::unique_ptr<Expr> Parser::parseBinary(int exprPrec, std::unique_ptr<Expr> lhs) {
    while (true) {
        int rangePrec = getPrecedence(currentToken.kind);
        if (rangePrec < exprPrec) return lhs;
        
        Token opToken = currentToken;
        advance();
        
        auto rhs = parsePrimary();
        int nextPrec = getPrecedence(currentToken.kind);
        if (rangePrec < nextPrec) {
            rhs = parseBinary(rangePrec + 1, std::move(rhs));
        }
        
        lhs = std::make_unique<BinaryExpr>(std::string(opToken.text), std::move(lhs), std::move(rhs));
    }
}

std::string Parser::parseTypeName() {
    std::string type = std::string(consume(TokenKind::Identifier, "Expected type name").text);
    while (match(TokenKind::LBracket)) {
        consume(TokenKind::RBracket, "Expected ']' after '[' in type name");
        type += "[]";
    }
    return type;
}

} // namespace pynext

