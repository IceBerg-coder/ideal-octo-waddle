#ifndef PYNEXT_AST_H
#define PYNEXT_AST_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "../sema/Type.h"

namespace pynext {

struct LiteralExpr;
struct VariableExpr;
struct BinaryExpr;
struct CallExpr;
struct MemberAccessExpr;
struct ReturnStmt;
struct Block;
struct IfStmt;
struct WhileStmt;
struct FunctionStmt;
struct VarDeclStmt;
struct StructDeclStmt;
struct ExprStmt;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(LiteralExpr& expr) = 0;
    virtual void visit(VariableExpr& expr) = 0;
    virtual void visit(BinaryExpr& expr) = 0;
    virtual void visit(CallExpr& expr) = 0;
    virtual void visit(MemberAccessExpr& expr) = 0;
    virtual void visit(ReturnStmt& stmt) = 0;
    virtual void visit(Block& stmt) = 0;
    virtual void visit(IfStmt& stmt) = 0;
    virtual void visit(WhileStmt& stmt) = 0;
    virtual void visit(FunctionStmt& stmt) = 0;
    virtual void visit(VarDeclStmt& stmt) = 0;
    virtual void visit(StructDeclStmt& stmt) = 0;
    virtual void visit(ExprStmt& stmt) = 0;
};

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
    virtual void accept(ASTVisitor& visitor) = 0;
};

struct Expr : public ASTNode {
    std::shared_ptr<Type> type;
};
struct Stmt : public ASTNode {};

// --- Expressions ---

struct LiteralExpr : public Expr {
    std::string value; 
    bool isFloat;
    bool isBool;
    bool isString;
    
    LiteralExpr(std::string val, bool isFloat, bool isBool = false, bool isString = false) 
        : value(std::move(val)), isFloat(isFloat), isBool(isBool), isString(isString) {}

    void print(int indent) const override {
        std::string typeStr = isString ? " (string)" : (isBool ? " (bool)" : "");
        std::cout << std::string(indent, ' ') << "Literal: " << value << typeStr << "\n";
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct VariableExpr : public Expr {
    std::string name;
    VariableExpr(std::string name) : name(std::move(name)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "Variable: " << name << "\n";
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct BinaryExpr : public Expr {
    std::string op; 
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::string op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right)
        : op(std::move(op)), left(std::move(left)), right(std::move(right)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "BinaryExpr (" << op << ")\n";
        left->print(indent + 2);
        right->print(indent + 2);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), args(std::move(args)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "CallExpr: " << callee << "\n";
        for (const auto& arg : args) {
            arg->print(indent + 2);
        }
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct MemberAccessExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::string member;

    MemberAccessExpr(std::unique_ptr<Expr> object, std::string member)
        : object(std::move(object)), member(std::move(member)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "MemberAccess: ." << member << "\n";
        object->print(indent + 2);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

// --- Statements ---

struct ExprStmt : public Stmt {
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {}
    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "ExprStmt\n";
        expr->print(indent + 2);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct ReturnStmt : public Stmt {
    std::unique_ptr<Expr> value; 

    ReturnStmt(std::unique_ptr<Expr> value) : value(std::move(value)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "ReturnStmt\n";
        if (value) value->print(indent + 2);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct Block : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    void print(int indent) const override {
        for (const auto& stmt : statements) {
            stmt->print(indent);
        }
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> thenBranch;
    std::unique_ptr<Block> elseBranch; 

    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Block> thenB, std::unique_ptr<Block> elseB)
        : condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "IfStmt\n";
        std::cout << std::string(indent + 2, ' ') << "Condition:\n";
        condition->print(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "Then:\n";
        thenBranch->print(indent + 4);
        if (elseBranch) {
            std::cout << std::string(indent + 2, ' ') << "Else:\n";
            elseBranch->print(indent + 4);
        }
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Block> body;

    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Block> body)
        : condition(std::move(cond)), body(std::move(body)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "WhileStmt\n";
        std::cout << std::string(indent + 2, ' ') << "Condition:\n";
        condition->print(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->print(indent + 4);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct VarDeclStmt : public Stmt {
    std::string name;
    std::string typeName; // optional
    std::unique_ptr<Expr> initializer;

    VarDeclStmt(std::string name, std::string typeName, std::unique_ptr<Expr> init)
        : name(std::move(name)), typeName(std::move(typeName)), initializer(std::move(init)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "VarDecl: " << name << " : " << (typeName.empty() ? "?" : typeName) << "\n";
        initializer->print(indent + 2);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct FunctionStmt : public Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // Name, Type
    std::string returnType;
    std::unique_ptr<Block> body;

    FunctionStmt(std::string name, 
                 std::vector<std::pair<std::string, std::string>> params, 
                 std::string returnType,
                 std::unique_ptr<Block> body)
        : name(std::move(name)), params(std::move(params)), returnType(std::move(returnType)), body(std::move(body)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "FunctionStmt: " << name << " -> " << returnType << "\n";
        std::cout << std::string(indent + 2, ' ') << "Params:\n";
        for (const auto& p : params) {
            std::cout << std::string(indent + 4, ' ') << p.first << ": " << p.second << "\n";
        }
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->print(indent + 4);
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

struct StructDeclStmt : public Stmt {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // Name, Type

    StructDeclStmt(std::string name, std::vector<std::pair<std::string, std::string>> fields)
        : name(std::move(name)), fields(std::move(fields)) {}

    void print(int indent) const override {
        std::cout << std::string(indent, ' ') << "StructDecl: " << name << "\n";
        for (const auto& f : fields) {
            std::cout << std::string(indent + 2, ' ') << f.first << ": " << f.second << "\n";
        }
    }
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

} // namespace pynext

#endif // PYNEXT_AST_H
