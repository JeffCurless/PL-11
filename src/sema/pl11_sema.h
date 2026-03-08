#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include "../ast/pl11_ast.h"

namespace pl11 {

// ============================================================
// Symbol kinds
// ============================================================

enum class SymbolKind { VARIABLE, CONSTANT, PROCEDURE, FUNCTION, PARAMETER };

// ============================================================
// Symbol table entry
// ============================================================

struct Symbol {
    std::string name;
    SymbolKind  kind;
    TypeSpec    type;

    // Procedure-specific
    std::vector<ParamDecl> params;
    TypeSpec               retType;

    // LLVM alloca pointer (set during codegen)
    void* allocaPtr = nullptr;  // llvm::AllocaInst*
};

// ============================================================
// Scope
// ============================================================

class Scope {
public:
    void declare(const std::string& name, Symbol sym);
    Symbol* lookup(const std::string& name);
    bool    localLookup(const std::string& name) const;

private:
    std::unordered_map<std::string, Symbol> symbols_;
};

// ============================================================
// Semantic error
// ============================================================

struct SemanticError : std::runtime_error {
    SourceLoc loc;
    SemanticError(const std::string& msg, SourceLoc l)
        : std::runtime_error(msg), loc(l) {}
};

// ============================================================
// Type checker / semantic analyser
// ============================================================

class Sema {
public:
    Sema();

    // Entry point — analyses entire program in place (modifies nothing, just checks)
    void analyse(ProgramNode& prog);

private:
    // Scope stack: back() is current (innermost) scope
    std::vector<std::unique_ptr<Scope>> scopes_;

    // Current procedure return type (VOID if in top-level block)
    TypeSpec curReturnType_;

    void pushScope();
    void popScope();
    void declare(const std::string& name, Symbol sym, SourceLoc loc);
    Symbol* lookup(const std::string& name, SourceLoc loc);

    // Analysis visitors
    void analyseBlock(BlockNode& block);
    void analyseDecl(ASTNode& decl);
    void analyseStmt(ASTNode& stmt);
    TypeSpec analyseExpr(ASTNode& expr);

    // Type compatibility
    bool compatible(const TypeSpec& a, const TypeSpec& b);
    bool isArithmetic(const TypeSpec& t);
    TypeSpec promoteArith(const TypeSpec& a, const TypeSpec& b);
    SemanticError typeError(const std::string& msg, SourceLoc loc);
};

} // namespace pl11
