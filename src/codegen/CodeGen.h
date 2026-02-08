#ifndef PYNEXT_CODEGEN_H
#define PYNEXT_CODEGEN_H

#include "../parser/AST.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <map>
#include <string>

namespace pynext {

class CodeGen : public ASTVisitor {
public:
    CodeGen(llvm::LLVMContext& context) 
        : context(context), builder(context) {
        module = std::make_unique<llvm::Module>("PyNextModule", context);
    }

    llvm::Module* getModule() const { return module.get(); }
    std::unique_ptr<llvm::Module> releaseModule() { return std::move(module); }
    void generate(const std::vector<std::unique_ptr<Stmt>>& stmts);

    // Visitor Implementation
    void visit(LiteralExpr& expr) override;
    void visit(VariableExpr& expr) override;
    void visit(BinaryExpr& expr) override;
    void visit(CallExpr& expr) override;
    void visit(ReturnStmt& stmt) override;
    void visit(Block& stmt) override;
    void visit(IfStmt& stmt) override;
    void visit(WhileStmt& stmt) override;
    void visit(ForStmt& stmt) override;
    void visit(FunctionStmt& stmt) override;
    void visit(VarDeclStmt& stmt) override;
    void visit(StructDeclStmt& stmt) override;
    void visit(ExprStmt& stmt) override;
    void visit(MemberAccessExpr& expr) override;
    void visit(IndexExpr& expr) override;
    void visit(ArrayLiteralExpr& expr) override;

    llvm::Value* getLastValue() { return lastValue; }

private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<> builder;
    std::unique_ptr<llvm::Module> module;
    
    std::map<std::string, llvm::AllocaInst*> namedValues;
    std::map<std::string, llvm::StructType*> structTypes;
    std::map<std::string, std::map<std::string, int>> structFieldIndices;
    llvm::Value* lastValue = nullptr;
    
    // Memory Management
    // Stack of scopes. Each scope contains list of variables (alloca pointers) to cleanup.
    // Pair: <AllocaInst*, isArray>
    std::vector<std::vector<std::pair<llvm::Value*, bool>>> scopeStack; 

    // Helpers
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fun, const std::string& varName, llvm::Type* type);
    llvm::Type* getType(const std::string& typeName);
    llvm::Value* getLValueAddress(Expr* expr);
};

} // namespace pynext

#endif // PYNEXT_CODEGEN_H
