cmake_minimum_required(VERSION 3.20)
project(pynext VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Main Compiler Executable
add_executable(pynext 
    src/main.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/codegen/CodeGen.cpp
    src/sema/TypeChecker.cpp
)

# Link against LLVM core libraries
llvm_map_components_to_libnames(llvm_libs support core irreader executionengine mcjit native interpreter)
target_link_libraries(pynext PRIVATE ${llvm_libs})

# Enable testing
enable_testing()
add_subdirectory(tests)
