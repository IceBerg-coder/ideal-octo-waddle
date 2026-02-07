#ifndef PYNEXT_SEMA_H
#define PYNEXT_SEMA_H

#include "../parser/AST.h"
#include "Type.h"
#include <map>
#include <memory>
#include <string>

namespace pynext {

class TypeChecker : public ASTVisitor {
public:
    void check(const std::vector<std::unique_ptr<Stmt>>& stmts);

    // Visitor
    void visit(LiteralExpr& expr) override;
    void visit(VariableExpr& expr) override;
    void visit(BinaryExpr& expr) override;
    void visit(CallExpr& expr) override;
    void visit(ReturnStmt& stmt) override;
    void visit(Block& stmt) override;
    void visit(IfStmt& stmt) override;
    void visit(WhileStmt& stmt) override;
    void visit(FunctionStmt& stmt) override;
    void visit(VarDeclStmt& stmt) override;
    void visit(StructDeclStmt& stmt) override;
    void visit(ExprStmt& stmt) override;
    void visit(MemberAccessExpr& expr) override;

private:
    std::map<std::string, std::shared_ptr<Type>> symbolTable;
    std::shared_ptr<Type> currentFunctionReturnType;
    std::map<std::string, std::shared_ptr<StructType>> structDefs;
    
    std::shared_ptr<Type> resolveType(const std::string& name);
};

} // namespace pynext

#endif // PYNEXT_SEMA_H
