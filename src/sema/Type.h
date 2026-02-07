#ifndef PYNEXT_TYPE_H
#define PYNEXT_TYPE_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace pynext {

enum class TypeKind {
    Void,
    Int,
    Float,
    Bool,
    String,
    Struct,
    Function,
    TypeVariable
};

struct Type {
    TypeKind kind;
    explicit Type(TypeKind kind) : kind(kind) {}
    virtual ~Type() = default;
    virtual std::string toString() const = 0;
};

struct VoidType : public Type {
    VoidType() : Type(TypeKind::Void) {}
    std::string toString() const override { return "void"; }
};

struct IntType : public Type {
    IntType() : Type(TypeKind::Int) {}
    std::string toString() const override { return "int"; }
};

struct FloatType : public Type {
    FloatType() : Type(TypeKind::Float) {}
    std::string toString() const override { return "float"; }
};

struct BoolType : public Type {
    BoolType() : Type(TypeKind::Bool) {}
    std::string toString() const override { return "bool"; }
};

struct StringType : public Type {
    StringType() : Type(TypeKind::String) {}
    std::string toString() const override { return "string"; }
};

struct StructType : public Type {
    std::string name;
    std::vector<std::pair<std::string, std::shared_ptr<Type>>> fields;

    StructType(std::string name, std::vector<std::pair<std::string, std::shared_ptr<Type>>> fields)
        : Type(TypeKind::Struct), name(std::move(name)), fields(std::move(fields)) {}

    std::string toString() const override { return "struct " + name; }
    
    std::shared_ptr<Type> getMemberType(const std::string& memberName) {
        for (const auto& f : fields) {
            if (f.first == memberName) return f.second;
        }
        return nullptr;
    }
    
    int getMemberIndex(const std::string& memberName) {
        for (int i = 0; i < (int)fields.size(); ++i) {
            if (fields[i].first == memberName) return i;
        }
        return -1;
    }
};

struct FunctionType : public Type {
    std::shared_ptr<Type> returnType;
    std::vector<std::shared_ptr<Type>> paramTypes;

    FunctionType(std::shared_ptr<Type> ret, std::vector<std::shared_ptr<Type>> params)
        : Type(TypeKind::Function), returnType(std::move(ret)), paramTypes(std::move(params)) {}

    std::string toString() const override { return "function"; }
};

} // namespace pynext

#endif // PYNEXT_TYPE_H
