#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>

// LLVM headers
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Type.h>

#include "../ast/pl11_ast.h"
#include "../sema/pl11_sema.h"

namespace pl11 {

// ============================================================
// Code generation error
// ============================================================

struct CodegenError : std::runtime_error {
    SourceLoc loc;
    CodegenError(const std::string& msg, SourceLoc l = {})
        : std::runtime_error(msg), loc(l) {}
};

// ============================================================
// Symbol table entry for codegen
// ============================================================

struct CodegenSymbol {
    TypeSpec          type;
    SymbolKind        kind;
    llvm::Type*       elemType = nullptr; // LLVM type of the variable's value
    llvm::AllocaInst* alloca   = nullptr; // local variable alloca (nullptr for globals)
    llvm::GlobalVariable* global = nullptr;
    llvm::Function*   func     = nullptr; // for procedures/functions
};

// ============================================================
// Codegen scope
// ============================================================

class CodegenScope {
public:
    void set(const std::string& name, CodegenSymbol sym);
    CodegenSymbol* lookup(const std::string& name);

private:
    std::unordered_map<std::string, CodegenSymbol> syms_;
};

// ============================================================
// LLVM IR Code Generator
// ============================================================

class Codegen {
public:
    Codegen(llvm::LLVMContext& ctx, const std::string& moduleName);

    // Generates IR for the entire program.
    void generate(ProgramNode& prog);

    // Returns the LLVM module (ownership kept by Codegen).
    llvm::Module* getModule() { return module_.get(); }

    // Emit IR as text to stdout.
    void emitIR();

private:
    llvm::LLVMContext&               ctx_;
    std::unique_ptr<llvm::Module>    module_;
    llvm::IRBuilder<>                builder_;

    // Scope stack
    std::vector<std::unique_ptr<CodegenScope>> scopes_;

    // Current function being generated
    llvm::Function* curFunc_ = nullptr;

    // Label map for GOTO support (function-scoped)
    std::unordered_map<std::string, llvm::BasicBlock*> labels_;

    // PDP-11 register globals (R0-R15) — created on demand
    std::array<llvm::GlobalVariable*, 16> regGlobals_{};

    // Simulated PDP-11 stack for PUSH/POP (256 words, index-based)
    // Created on first PUSH or POP encountered.
    llvm::GlobalVariable* simStack_    = nullptr;  // [256 x i16]
    llvm::GlobalVariable* simStackIdx_ = nullptr;  // i32, initialised to 256 (empty)
    void ensureSimStack();

    // printf declaration — created on demand
    llvm::FunctionCallee getPrintf();

    void pushScope();
    void popScope();
    void setSymbol(const std::string& name, CodegenSymbol sym);
    CodegenSymbol* lookupSymbol(const std::string& name);

    // Type mapping
    llvm::Type* llvmType(const TypeSpec& t);
    llvm::Type* llvmIntType(int bytes);

    // Alloca helper — always inserted at function entry block
    llvm::AllocaInst* createEntryAlloca(llvm::Function* f, const std::string& name,
                                         llvm::Type* ty);

    // Register globals
    llvm::GlobalVariable* getRegGlobal(int regNum);

    // IR generation
    void genBlock(BlockNode& block);
    void genDecl(ASTNode& decl);
    void genStmt(ASTNode& stmt);
    llvm::Value* genExpr(ASTNode& expr);

    // Address (lvalue) generation — returns a pointer to the storage location
    llvm::Value* genLValue(ASTNode& expr);
};

} // namespace pl11
