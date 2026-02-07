#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "codegen/CodeGen.h"
#include "sema/TypeChecker.h"

#include <cstdio>

extern "C" void print_int(int64_t val) {
    printf("Output: %ld\n", val);
    fflush(stdout);
}

extern "C" void print_string(const char* val) {
    printf("Output: %s\n", val);
    fflush(stdout);
}

void executeSource(const std::string& code) {
    pynext::Lexer lexer(code);
    pynext::Parser parser(lexer);
    
    auto statements = parser.parseModule();
    
    pynext::TypeChecker checker;
    checker.check(statements);
    
    llvm::LLVMContext context;
    pynext::CodeGen codegen(context);
    codegen.generate(statements);
    
    llvm::outs() << "Generated LLVM IR:\n";
    codegen.getModule()->print(llvm::outs(), nullptr);
    llvm::outs() << "\n";
    
    // Cache function pointers before transferring module ownership
    llvm::Function* irPrintFunc = codegen.getModule()->getFunction("print_int");
    llvm::Function* irPrintStringFunc = codegen.getModule()->getFunction("print_string");
    llvm::Function* irMainFunc = codegen.getModule()->getFunction("main");

    // Initialize JIT
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);

    std::string errStr;
    // releaseModule() passes ownership to the Engine
    llvm::ExecutionEngine* engine = llvm::EngineBuilder(codegen.releaseModule())
                                        .setErrorStr(&errStr)
                                        .setEngineKind(llvm::EngineKind::JIT)
                                        .create();

    if (!engine) {
        std::cerr << "Failed to construct ExecutionEngine: " << errStr << "\n";
        return;
    }
    
    // Explicitly map print_int
    if (irPrintFunc) {
        engine->addGlobalMapping(irPrintFunc, (void*)print_int);
    } else {
        std::cerr << "Warning: print_int not found in module (CodeGen)\n";
    }

    if (irPrintStringFunc) {
        engine->addGlobalMapping(irPrintStringFunc, (void*)print_string);
    }
    
    if (!irMainFunc) {
        std::cerr << "Function 'main' not found in module.\n";
        return;
    }

    std::vector<llvm::GenericValue> args;
    engine->runFunction(irMainFunc, args);
}

void runTest() {
    std::string code = R"(
        extern def print_int(val: int)
        
        def fib(n: int) -> int
            if n < 2
                return n
            end
            return fib(n-1) + fib(n-2)
        end
        
        def main()
            print_int(fib(10))
        end
    )";
    
    llvm::outs() << "Running Internal Test:\n" << code << "\n\n";
    executeSource(code);
}

void runFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    executeSource(buffer.str());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        llvm::outs() << "Usage: pynext <file.next> or pynext test\n";
        return 0;
    }

    std::string arg1 = argv[1];
    if (arg1 == "test") {
        runTest();
    } else {
        runFile(arg1);
    }

    return 0;
}
