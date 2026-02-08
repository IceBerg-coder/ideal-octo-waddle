#include "TypeChecker.h"

namespace pynext {

std::shared_ptr<Type> TypeChecker::resolveType(const std::string& name) {
    if (name == "int") return std::make_shared<IntType>();
    if (name == "float") return std::make_shared<FloatType>();
    if (name == "bool") return std::make_shared<BoolType>();
    if (name == "string") return std::make_shared<StringType>();
    if (name == "void") return std::make_shared<VoidType>();
    
    if (structDefs.count(name)) return structDefs[name];
    
    // Array Types (e.g., int[], Point[][])
    if (name.length() > 2 && name.substr(name.length() - 2) == "[]") {
        std::string elemName = name.substr(0, name.length() - 2);
        return std::make_shared<ArrayType>(resolveType(elemName));
    }
    
    return std::make_shared<VoidType>(); // Default/Error
}

void TypeChecker::check(const std::vector<std::unique_ptr<Stmt>>& stmts) {
    for (const auto& stmt : stmts) {
        stmt->accept(*this);
    }
}

void TypeChecker::visit(LiteralExpr& expr) {
    if (expr.isBool) {
        expr.type = std::make_shared<BoolType>();
    } else if (expr.isString) {
        expr.type = std::make_shared<StringType>();
    } else if (expr.isFloat) {
        expr.type = std::make_shared<FloatType>();
    } else {
        expr.type = std::make_shared<IntType>();
    }
}

void TypeChecker::visit(VariableExpr& expr) {
    if (symbolTable.count(expr.name)) {
        expr.type = symbolTable[expr.name];
    } else {
        std::cerr << "Type Error: Undefined variable '" << expr.name << "'\n";
        expr.type = std::make_shared<VoidType>();
    }
}

void TypeChecker::visit(BinaryExpr& expr) {
    // Assignment Logic
    if (expr.op == "=") {
        // LHS can be VariableExpr or MemberAccessExpr or IndexExpr
        bool isValidLHS = false;
        if (dynamic_cast<VariableExpr*>(expr.left.get())) isValidLHS = true;
        else if (dynamic_cast<MemberAccessExpr*>(expr.left.get())) isValidLHS = true;
        else if (dynamic_cast<IndexExpr*>(expr.left.get())) isValidLHS = true;

        if (!isValidLHS) {
            std::cerr << "Type Error: Assignment to non-lvalue\n";
            expr.type = std::make_shared<VoidType>();
        } else {
            expr.left->accept(*this); // Resolve LHS type (and validate members)
            expr.right->accept(*this);
            // TODO: strictly check expr.left->type == expr.right->type
            expr.type = expr.right->type;
        }
        return;
    }

    expr.left->accept(*this);
    expr.right->accept(*this);
    
    // Simplistic rule: Int + Int = Int
    // TODO: Add strict checking
    if (expr.left->type->kind == TypeKind::Int && expr.right->type->kind == TypeKind::Int) {
        expr.type = std::make_shared<IntType>();
    } else {
        // Fallback
        expr.type = expr.left->type;
    }
}

void TypeChecker::visit(CallExpr& expr) {
    // Check args
    for (auto& arg : expr.args) {
        arg->accept(*this);
    }
    
    if (symbolTable.count(expr.callee)) {
        auto type = symbolTable[expr.callee];
        if (auto ft = std::dynamic_pointer_cast<FunctionType>(type)) {
            expr.type = ft->returnType;
        } else {
            std::cerr << "Type Error: '" << expr.callee << "' is not a function\n";
            expr.type = std::make_shared<VoidType>();
        }
    } else {
        std::cerr << "Type Error: Undefined function '" << expr.callee << "'\n";
        expr.type = std::make_shared<VoidType>();
    }
}

void TypeChecker::visit(ReturnStmt& stmt) {
    if (stmt.value) {
        stmt.value->accept(*this);
        // Check match with currentFunctionReturnType
        // ...
    }
}

void TypeChecker::visit(Block& stmt) {
    for (const auto& s : stmt.statements) {
        s->accept(*this);
    }
}

void TypeChecker::visit(IfStmt& stmt) {
    stmt.condition->accept(*this);
    stmt.thenBranch->accept(*this);
    if (stmt.elseBranch) stmt.elseBranch->accept(*this);
}

void TypeChecker::visit(WhileStmt& stmt) {
    stmt.condition->accept(*this);
    // TODO: Verify condition is boolean/int
    stmt.body->accept(*this);
}

void TypeChecker::visit(FunctionStmt& stmt) {
    // 1. Register Function in Symbol Table (Global)
    std::vector<std::shared_ptr<Type>> paramTypes;
    for (const auto& p : stmt.params) {
        paramTypes.push_back(resolveType(p.second));
    }
    auto returnType = resolveType(stmt.returnType);
    
    auto funcType = std::make_shared<FunctionType>(returnType, paramTypes);
    symbolTable[stmt.name] = funcType;
    
    if (!stmt.body) return; // Extern

    // 2. Enter Scope (For now, just add params to global table temporarily - BAD but simple)
    // TODO: Implement proper Scoping
    currentFunctionReturnType = returnType;
    
    auto oldTable = symbolTable; // Snapshot (Cheap copy of map?) No, deep copy of map structure.
    
    for (size_t i = 0; i < stmt.params.size(); ++i) {
        symbolTable[stmt.params[i].first] = paramTypes[i];
    }
    
    stmt.body->accept(*this);
    
    // Restore Scope
    symbolTable = oldTable;
    // Re-add function itself (snapshot removed it if it was added this cycle?)
    // Actually, if we added it before snapshot, it's in snapshot. 
    // Wait, I added it to `symbolTable` before snapshot. So preserving is fine.
}

void TypeChecker::visit(VarDeclStmt& stmt) {
    std::shared_ptr<Type> type;
    
    if (stmt.initializer) {
        stmt.initializer->accept(*this);
        if (!stmt.typeName.empty()) {
            type = resolveType(stmt.typeName);
             // TODO: Check compatibility with stmt.initializer->type
        } else {
            type = stmt.initializer->type;
        }
    } else {
        if (!stmt.typeName.empty()) {
            type = resolveType(stmt.typeName);
        } else {
             std::cerr << "Type Error: Variable declaration missing type and initializer\n";
             type = std::make_shared<VoidType>();
        }
    }
    
    symbolTable[stmt.name] = type;
}

void TypeChecker::visit(StructDeclStmt& stmt) {
    // Two pass? 
    // First, register the name (if we allow recursive structs, we need pointer types first. we don't yet).
    // So just parse fields.
    std::vector<std::pair<std::string, std::shared_ptr<Type>>> fields;
    
    for (auto& f : stmt.fields) {
        // If field type is the struct itself, this will fail/recurse poorly without pointers.
        // Assuming simple composition for now.
        fields.push_back({f.first, resolveType(f.second)});
    }
    
    auto st = std::make_shared<StructType>(stmt.name, fields);
    structDefs[stmt.name] = st;
}

void TypeChecker::visit(MemberAccessExpr& expr) {
    expr.object->accept(*this);
    auto objType = expr.object->type;
    
    auto structType = std::dynamic_pointer_cast<StructType>(objType);
    if (!structType) {
        std::cerr << "Type Error: Member access on non-struct\n";
        expr.type = std::make_shared<VoidType>();
        return;
    }
    
    auto memberType = structType->getMemberType(expr.member);
    if (!memberType) {
        std::cerr << "Type Error: Struct '" << structType->name << "' has no member '" << expr.member << "'\n";
        expr.type = std::make_shared<VoidType>();
    } else {
        expr.type = memberType;
    }
}

void TypeChecker::visit(ExprStmt& stmt) {
    stmt.expr->accept(*this);
}

void TypeChecker::visit(IndexExpr& expr) {
    expr.object->accept(*this);
    expr.index->accept(*this);
    
    // Check if object is array
    auto arrType = std::dynamic_pointer_cast<ArrayType>(expr.object->type);
    if (!arrType) {
        std::cerr << "Type Error: Indexing non-array type\n";
        expr.type = std::make_shared<VoidType>();
        return;
    }
    
    // Check if index is int
    if (expr.index->type->kind != TypeKind::Int) {
        std::cerr << "Type Error: Array index must be integer\n";
    }
    
    expr.type = arrType->elementType;
}

void TypeChecker::visit(ArrayLiteralExpr& expr) {
    if (expr.elements.empty()) {
        // Empty array... type is Array<Void>? Or inferred later?
        // Let's assume Void for now or Error.
        // Actually, let's use a special "Any" or "Unknown" if we had it.
        // For now: Array<Int> default?
        expr.type = std::make_shared<ArrayType>(std::make_shared<IntType>()); 
        return;
    }
    
    // Check all elements are same type
    expr.elements[0]->accept(*this);
    auto firstType = expr.elements[0]->type;
    
    for (size_t i = 1; i < expr.elements.size(); ++i) {
        expr.elements[i]->accept(*this);
        // Strict equality check?
        // TODO: implement strict type equality
    }
    
    expr.type = std::make_shared<ArrayType>(firstType);
}

} // namespace pynext
