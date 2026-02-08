#include "CodeGen.h"
#include <iostream>

namespace pynext {

void CodeGen::generate(const std::vector<std::unique_ptr<Stmt>>& stmts) {
    // Check for user-defined main
    bool hasUserMain = false;
    for (const auto& s : stmts) {
        if (auto f = dynamic_cast<FunctionStmt*>(s.get())) {
            if (f->name == "main") hasUserMain = true;
        }
    }

    // Create entry function (default "main", or "__init" if main exists)
    std::string entryName = hasUserMain ? "__init" : "main";
    
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(context), false);
    llvm::Function* entryFunc = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, entryName, module.get());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", entryFunc);
    builder.SetInsertPoint(bb);

    for (const auto& stmt : stmts) {
        stmt->accept(*this);
    }
    
    // Ensure entry function returns
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(64, 0)));
    }
}

llvm::Type* CodeGen::getType(const std::string& typeName) {
    if (typeName == "int") return llvm::Type::getInt64Ty(context);
    if (typeName == "float") return llvm::Type::getDoubleTy(context);
    if (typeName == "bool") return llvm::Type::getInt1Ty(context);
    if (typeName == "string") return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    if (typeName == "void") return llvm::Type::getVoidTy(context);
    
    // Check for array syntax
    if (typeName.length() > 2 && typeName.substr(typeName.length() - 2) == "[]") {
         std::string elemName = typeName.substr(0, typeName.length() - 2);
         llvm::Type* elemType = getType(elemName);
         // Array representation: Pointer to array struct { size, elements* }? 
         // Or C-style raw pointer?
         // Let's do raw pointer for simpler interop for now, as step 1.
         // Or better: { i64 size, T* elements }
         
         // IMPLEMENTATION CHOICE:
         // To support `arr[i]`, we need to know the stride.
         // If we just use T*, LLVM handles stride.
         // But we loose size safety.
         
         // Let's use T* for now (Unsafe/Simple)
         return llvm::PointerType::get(elemType, 0); 
    }

    if (structTypes.count(typeName)) return structTypes[typeName];
    return llvm::Type::getInt64Ty(context); // Default
}

llvm::AllocaInst* CodeGen::createEntryBlockAlloca(llvm::Function* fun, const std::string& varName, llvm::Type* type) {
    llvm::IRBuilder<> tmpB(&fun->getEntryBlock(), fun->getEntryBlock().begin());
    return tmpB.CreateAlloca(type, nullptr, varName);
}

void CodeGen::visit(LiteralExpr& expr) {
    if (expr.isBool) {
        lastValue = llvm::ConstantInt::get(context, llvm::APInt(1, expr.value == "true" ? 1 : 0));
    } else if (expr.isString) {
        lastValue = builder.CreateGlobalStringPtr(expr.value);
    } else if (expr.isFloat) {
        lastValue = llvm::ConstantFP::get(context, llvm::APFloat(std::stod(expr.value)));
    } else {
        lastValue = llvm::ConstantInt::get(context, llvm::APInt(64, std::stoll(expr.value), true));
    }
}

void CodeGen::visit(VariableExpr& expr) {
    llvm::AllocaInst* alloca = namedValues[expr.name];
    if (!alloca) {
        std::cerr << "Unknown variable name: " << expr.name << "\n";
        lastValue = nullptr;
        return;
    }
    lastValue = builder.CreateLoad(alloca->getAllocatedType(), alloca, expr.name.c_str());
}

void CodeGen::visit(BinaryExpr& expr) {
    // Special handling for Assignment
    if (expr.op == "=") {
        llvm::Value* lvalAddr = getLValueAddress(expr.left.get());
        if (!lvalAddr) {
            std::cerr << "Error: Invalid l-value in assignment\n";
            lastValue = nullptr;
            return;
        }

        // Generate RHS
        expr.right->accept(*this);
        llvm::Value* val = lastValue;
        if (!val) return;

        builder.CreateStore(val, lvalAddr);
        // Assignment result is the value
        lastValue = val;
        return;
    }

    expr.left->accept(*this);
    llvm::Value* l = lastValue;
    expr.right->accept(*this);
    llvm::Value* r = lastValue;

    if (!l || !r) {
        lastValue = nullptr;
        return;
    }

    if (expr.op == "+") lastValue = builder.CreateAdd(l, r, "addtmp");
    else if (expr.op == "-") lastValue = builder.CreateSub(l, r, "subtmp");
    else if (expr.op == "*") lastValue = builder.CreateMul(l, r, "multmp");
    else if (expr.op == "/") lastValue = builder.CreateSDiv(l, r, "divtmp"); // Signed div
    else if (expr.op == "<") {
        lastValue = builder.CreateICmpSLT(l, r, "cmptmp");
        // Convert bool to int 0/1 if needed or keep as i1
        // Usually logical ops return i1 in LLVM
    }
    else if (expr.op == ">") lastValue = builder.CreateICmpSGT(l, r, "cmptmp");
    else if (expr.op == "==") lastValue = builder.CreateICmpEQ(l, r, "cmptmp");
    else if (expr.op == "!=") lastValue = builder.CreateICmpNE(l, r, "cmptmp");
    else {
        std::cerr << "Unknown operator: " << expr.op << "\n";
        lastValue = nullptr;
    }
}

void CodeGen::visit(CallExpr& expr) {
    llvm::Function* callee = module->getFunction(expr.callee);
    if (!callee) {
        std::cerr << "Unknown function referenced: " << expr.callee << "\n";
        lastValue = nullptr;
        return;
    }

    if (callee->arg_size() != expr.args.size()) {
        std::cerr << "Incorrect # arguments passed\n";
        lastValue = nullptr;
        return;
    }

    std::vector<llvm::Value*> argsV;
    for (auto& arg : expr.args) {
        arg->accept(*this);
        if (!lastValue) return;
        argsV.push_back(lastValue);
    }

    lastValue = builder.CreateCall(callee, argsV, "calltmp");
}

void CodeGen::visit(ReturnStmt& stmt) {
    if (stmt.value) {
        stmt.value->accept(*this);
        builder.CreateRet(lastValue);
    } else {
        builder.CreateRetVoid();
    }
}

void CodeGen::visit(Block& stmt) {
    for (const auto& s : stmt.statements) {
        s->accept(*this);
    }
}

void CodeGen::visit(IfStmt& stmt) {
    stmt.condition->accept(*this);
    if (!lastValue) return;
    
    llvm::Value* condV = lastValue;
    // Ensure condition is boolean i1
    if (condV->getType()->isIntegerTy(64)) {
        condV = builder.CreateICmpNE(condV, llvm::ConstantInt::get(context, llvm::APInt(64, 0)), "ifcond");
    }

    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", func);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont");

    bool hasElse = (stmt.elseBranch != nullptr);

    builder.CreateCondBr(condV, thenBB, hasElse ? elseBB : mergeBB);

    // Emit Then
    builder.SetInsertPoint(thenBB);
    stmt.thenBranch->accept(*this);
    
    // Check if the block already has a terminator (like return)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(mergeBB);
    }

    // Emit Else
    if (hasElse) {
        func->insert(func->end(), elseBB);
        builder.SetInsertPoint(elseBB);
        stmt.elseBranch->accept(*this);
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(mergeBB);
        }
    } else {
        // If no else, the condBr jumped to mergeBB, so we don't need elseBB
    }

    func->insert(func->end(), mergeBB);
    builder.SetInsertPoint(mergeBB);
}
void CodeGen::visit(WhileStmt& stmt) {
    llvm::Function* func = builder.GetInsertBlock()->getParent();

    // 1. Create Basic Blocks
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "whilecond", func);
    llvm::BasicBlock* loopBB = llvm::BasicBlock::Create(context, "whilebody");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "afterwhile");

    // 2. Jump to Condition
    builder.CreateBr(condBB);

    // 3. Emit Condition
    builder.SetInsertPoint(condBB);
    stmt.condition->accept(*this);
    if (!lastValue) return;

    llvm::Value* condV = lastValue;
    if (condV->getType()->isIntegerTy(64)) {
        condV = builder.CreateICmpNE(condV, llvm::ConstantInt::get(context, llvm::APInt(64, 0)), "loopcond");
    }

    builder.CreateCondBr(condV, loopBB, afterBB);

    // 4. Emit Loop Body
    func->insert(func->end(), loopBB);
    builder.SetInsertPoint(loopBB);
    
    stmt.body->accept(*this);
    
    // Jump back to condition if no terminator (fix flow)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(condBB);
    }

    // 5. Continue
    func->insert(func->end(), afterBB);
    builder.SetInsertPoint(afterBB);
}

void CodeGen::visit(VarDeclStmt& stmt) {
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    llvm::Value* initVal = nullptr;
    if (stmt.initializer) {
        stmt.initializer->accept(*this);
        initVal = lastValue; 
    }
    
    llvm::Type* varType = nullptr;
    if (!stmt.typeName.empty()) {
        varType = getType(stmt.typeName);
    } else if (initVal) {
        varType = initVal->getType();
    } else {
        // Should be caught by sema
        return;
    }
    
    // 2. Alloca
    llvm::AllocaInst* alloca = createEntryBlockAlloca(func, stmt.name, varType);
    
    if (initVal) {
        builder.CreateStore(initVal, alloca);
    } else {
        // Zero init
        builder.CreateStore(llvm::Constant::getNullValue(varType), alloca);
    }
    
    // 3. Register info
    namedValues[stmt.name] = alloca;
}

void CodeGen::visit(ExprStmt& stmt) {
    stmt.expr->accept(*this);
}

void CodeGen::visit(FunctionStmt& stmt) {
    // 1. Prototype
    std::vector<llvm::Type*> argTypes;
    for (const auto& p : stmt.params) {
        argTypes.push_back(getType(p.second));
    }
    
    llvm::Type* retType = getType(stmt.returnType);
    llvm::FunctionType* ft = llvm::FunctionType::get(retType, argTypes, false);
    llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, stmt.name, module.get());

    if (!stmt.body) return; // Extern declaration

    // 2. Body
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", func);
    
    // Save previous block and context
    llvm::BasicBlock* oldBB = builder.GetInsertBlock();
    auto oldNamedValues = namedValues;
    
    builder.SetInsertPoint(bb);
    namedValues.clear();
    
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        std::string argName = stmt.params[idx].first;
        arg.setName(argName);
        
        // Create Stack var
        llvm::AllocaInst* alloca = createEntryBlockAlloca(func, argName, arg.getType());
        builder.CreateStore(&arg, alloca);
        namedValues[argName] = alloca;
        idx++;
    }
    
    stmt.body->accept(*this);
    
    // Auto-insert return void if missing
    llvm::BasicBlock* curBB = builder.GetInsertBlock();
    if (!curBB->getTerminator()) {
        if (stmt.returnType == "void") {
            builder.CreateRetVoid();
        } else {
            // For non-void, returning 0/undef is better than crashing
            // But ideally we should sema check this.
            // For now, return 0.
            llvm::Type* retTy = func->getReturnType();
            if (retTy->isIntegerTy()) {
                builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(retTy->getIntegerBitWidth(), 0)));
            } else {
                builder.CreateRet(llvm::UndefValue::get(retTy));
            }
        }
    }
    
    llvm::verifyFunction(*func);
    
    // Restore context
    if (oldBB) builder.SetInsertPoint(oldBB);
    namedValues = oldNamedValues;
}

void CodeGen::visit(StructDeclStmt& stmt) {
    if (structTypes.count(stmt.name)) return;

    std::vector<llvm::Type*> fieldTypes;
    std::map<std::string, int> fieldIndices;
    int idx = 0;
    for (const auto& field : stmt.fields) {
        fieldTypes.push_back(getType(field.second));
        fieldIndices[field.first] = idx++;
    }

    llvm::StructType* structTy = llvm::StructType::create(context, fieldTypes, stmt.name);
    structTypes[stmt.name] = structTy;
    structFieldIndices[stmt.name] = fieldIndices;
}

void CodeGen::visit(MemberAccessExpr& expr) {
    llvm::Value* addr = getLValueAddress(&expr);
    if (!addr) {
        lastValue = nullptr;
        return;
    }
    // Load the value (R-value access)
    // addr is a pointer to the field.
    // We need to know the type being loaded.
    // The type can be inferred from the pointer element type?
    // No, addr is an opaque ptr.
    // We should use expr.type if populated.
    
    llvm::Type* loadType = nullptr;
    if (expr.type) {
        // Map Sema Type to LLVM Type
        // We need a helper for Type -> llvm::Type
        // Currently getType takes string.
        // But StructType in Sema has name.
        if (expr.type->kind == TypeKind::Int) loadType = llvm::Type::getInt64Ty(context);
        else if (expr.type->kind == TypeKind::Float) loadType = llvm::Type::getDoubleTy(context);
        else if (expr.type->kind == TypeKind::Bool) loadType = llvm::Type::getInt1Ty(context);
        else if (expr.type->kind == TypeKind::String) loadType = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
        else if (auto st = std::dynamic_pointer_cast<pynext::StructType>(expr.type)) {
            if (structTypes.count(st->name)) loadType = structTypes[st->name];
        }
    }
    
    if (!loadType) {
        // Fallback: try to deduce from LValue address generation? 
        // We know the struct type and index.
        // We can get field type from struct definition.
        // Re-do lookups.
        // ... (Implement simpler logic that recalculates)
        
        // Let's assume Sema worked. If not, we might crash on Load.
        // Try Int64 as fallback purely to avoid segfault right here.
        loadType = llvm::Type::getInt64Ty(context);
    }
    
    lastValue = builder.CreateLoad(loadType, addr, "memberload");
}

void CodeGen::visit(IndexExpr& expr) {
    // 1. Calculate Address
    // This logic duplicates getLValueAddress mostly, but...
    // Let's use getLValueAddress to get the pointer to the element.
    // If we are reading, we load.
    
    llvm::Value* addr = getLValueAddress(&expr);
    if (!addr) {
        lastValue = nullptr;
        return;
    }
    
    // 2. Load
    // We need the type.
    llvm::Type* loadType = llvm::Type::getInt64Ty(context); // default
    if (expr.type) {
         if (expr.type->kind == TypeKind::Int) loadType = llvm::Type::getInt64Ty(context);
         else if (expr.type->kind == TypeKind::Float) loadType = llvm::Type::getDoubleTy(context);
         else if (expr.type->kind == TypeKind::Bool) loadType = llvm::Type::getInt1Ty(context);
         else if (auto st = std::dynamic_pointer_cast<pynext::StructType>(expr.type)) {
             if (structTypes.count(st->name)) loadType = structTypes[st->name];
         }
    }
    
    lastValue = builder.CreateLoad(loadType, addr, "indexload");
}

void CodeGen::visit(ArrayLiteralExpr& expr) {
    // Create an Array on Heap? 
    // Malloc (size * sizeof(element))
    
    int size = expr.elements.size();
    
    // 1. Determine element size/type
    // Assume homogeneous based on first element or TypeChecker
    llvm::Type* elemType = llvm::Type::getInt64Ty(context);
    if (expr.type) {
         auto arrT = std::dynamic_pointer_cast<pynext::ArrayType>(expr.type);
         if (arrT && arrT->elementType) {
             // Map Type... (Refactor this mapping!)
             // Quick Mapping again:
             auto et = arrT->elementType;
             if (et->kind == TypeKind::Int) elemType = llvm::Type::getInt64Ty(context);
             else if (et->kind == TypeKind::Float) elemType = llvm::Type::getDoubleTy(context);
             else if (et->kind == TypeKind::Bool) elemType = llvm::Type::getInt1Ty(context);
             else if (auto st = std::dynamic_pointer_cast<pynext::StructType>(et)) {
                 if (structTypes.count(st->name)) elemType = structTypes[st->name];
             }
             // ...
         }
    }
    
    // 2. Malloc
    // We need `malloc` declared.
    llvm::Function* mallocFunc = module->getFunction("malloc");
    if (!mallocFunc) {
        // Declare malloc: i8* malloc(i64)
        std::vector<llvm::Type*> args = { llvm::Type::getInt64Ty(context) };
        llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0), args, false);
        mallocFunc = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "malloc", module.get());
    }
    
    // Size required?
    llvm::DataLayout dl(module.get());
    uint64_t typeSize = dl.getTypeAllocSize(elemType);
    uint64_t totalSize = typeSize * size;
    
    llvm::Value* sizeVal = llvm::ConstantInt::get(context, llvm::APInt(64, totalSize));
    llvm::Value* voidPtr = builder.CreateCall(mallocFunc, {sizeVal}, "malloccall");
    
    // Cast to T*
    llvm::Value* arrayPtr = voidPtr; // Opaque pointers in LLVM 18 don't need cast? 
    // Actually GEP needs correct type.
    
    // 3. Store elements
    for (int i=0; i < size; ++i) {
        expr.elements[i]->accept(*this);
        llvm::Value* val = lastValue;
        
        llvm::Value* idxVal = llvm::ConstantInt::get(context, llvm::APInt(64, i));
        llvm::Value* gep = builder.CreateGEP(elemType, arrayPtr, idxVal, "initidx");
        builder.CreateStore(val, gep);
    }
    
    lastValue = arrayPtr;
}

llvm::Value* CodeGen::getLValueAddress(Expr* expr) {
    if (auto varFn = dynamic_cast<VariableExpr*>(expr)) {
        if (namedValues.count(varFn->name)) {
            return namedValues[varFn->name];
        }
        std::cerr << "Unknown variable: " << varFn->name << "\n";
        return nullptr;
    } 
    
    if (auto memFn = dynamic_cast<MemberAccessExpr*>(expr)) {
        llvm::Value* base = getLValueAddress(memFn->object.get());
        if (!base) return nullptr;
        
        // Need to find which struct type 'base' points to.
        // Rely on Sema type info.
        if (!memFn->object->type) {
             std::cerr << "Internal Error: Type missing for object in member access\n";
             return nullptr;
        }
        
        auto structInfo = std::dynamic_pointer_cast<pynext::StructType>(memFn->object->type);
        if (!structInfo) {
            std::cerr << "Member access on non-struct type\n";
            return nullptr;
        }

        std::string structName = structInfo->name;
        if (structTypes.find(structName) == structTypes.end()) {
             std::cerr << "Unknown struct type in codegen: " << structName << "\n";
             return nullptr;
        }
        
        llvm::StructType* st = structTypes[structName];
        if (structFieldIndices[structName].count(memFn->member) == 0) {
             std::cerr << "Unknown member: " << memFn->member << "\n";
             return nullptr;
        }
        
        int idx = structFieldIndices[structName][memFn->member];
        
        llvm::Value* indices[] = {
            llvm::ConstantInt::get(context, llvm::APInt(32, 0)), // Dereference pointer
            llvm::ConstantInt::get(context, llvm::APInt(32, idx)) // Field index
        };
        
        return builder.CreateGEP(st, base, indices, "memberaddr");
    }
    
    if (auto idxExpr = dynamic_cast<IndexExpr*>(expr)) {
        llvm::Value* base = getLValueAddress(idxExpr->object.get()); // Recursion? No, see below.
        // Wait, getLValueAddress is for getting address of a VARIABLE.
        // If we have `a[i]`, we want value of `a` (pointer), then GEP.
        // But `a` is an LValue (VariableExpr). `getLValueAddress(a)` gives address of `a` (stack slot).
        // If we Load `a`, we get the array pointer.
        
        // My previous logic:
        // idxExpr->object->accept(*this);
        // llvm::Value* arrayPtr = lastValue;
        
        // Let's verify this logic.
        idxExpr->object->accept(*this);
        llvm::Value* arrayPtr = lastValue;
        
        if (!arrayPtr) {
            std::cerr << "CodeGen Error: Array base eval failed\n";
            return nullptr;
        }

        idxExpr->index->accept(*this);
        llvm::Value* indexVal = lastValue;
        
        if (!indexVal) {
             std::cerr << "CodeGen Error: Index eval failed\n";
             return nullptr;
        }
        
        if (!idxExpr->object->type) {
             std::cerr << "CodeGen Error: Array object has no type info\n";
             return nullptr;
        }

        if (auto arrType = std::dynamic_pointer_cast<pynext::ArrayType>(idxExpr->object->type)) {
             llvm::Type* elemLLVMType = nullptr;
             auto et = arrType->elementType;
             if (et->kind == TypeKind::Int) elemLLVMType = llvm::Type::getInt64Ty(context);
             else if (et->kind == TypeKind::Float) elemLLVMType = llvm::Type::getDoubleTy(context);
             else if (et->kind == TypeKind::Bool) elemLLVMType = llvm::Type::getInt1Ty(context);
             else if (auto st = std::dynamic_pointer_cast<pynext::StructType>(et)) {
                 if (structTypes.count(st->name)) elemLLVMType = structTypes[st->name];
             }
             
             if (!elemLLVMType) elemLLVMType = llvm::Type::getInt64Ty(context); 
             
             return builder.CreateGEP(elemLLVMType, arrayPtr, indexVal, "indexaddr");
        } else {
             std::cerr << "CodeGen Error: Object type is not ArrayType: " << idxExpr->object->type->toString() << "\n";
        }
    }
    
    return nullptr;
}

} // namespace pynext
