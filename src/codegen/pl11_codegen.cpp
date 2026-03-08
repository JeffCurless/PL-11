#include "pl11_codegen.h"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/InlineAsm.h>
#include <sstream>

namespace pl11 {

// ============================================================
// CodegenScope
// ============================================================

void CodegenScope::set(const std::string& name, CodegenSymbol sym) {
    syms_[name] = std::move(sym);
}

CodegenSymbol* CodegenScope::lookup(const std::string& name) {
    auto it = syms_.find(name);
    return it != syms_.end() ? &it->second : nullptr;
}

// ============================================================
// Codegen
// ============================================================

Codegen::Codegen(llvm::LLVMContext& ctx, const std::string& moduleName)
    : ctx_(ctx),
      module_(std::make_unique<llvm::Module>(moduleName, ctx)),
      builder_(ctx) {
    regGlobals_.fill(nullptr);
}

void Codegen::pushScope() { scopes_.push_back(std::make_unique<CodegenScope>()); }
void Codegen::popScope()  { scopes_.pop_back(); }

void Codegen::setSymbol(const std::string& name, CodegenSymbol sym) {
    scopes_.back()->set(name, std::move(sym));
}

CodegenSymbol* Codegen::lookupSymbol(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i)
        if (auto* s = scopes_[i]->lookup(name)) return s;
    return nullptr;
}

// ============================================================
// Type mapping
// ============================================================

llvm::Type* Codegen::llvmIntType(int bytes) {
    switch (bytes) {
    case 1:  return llvm::Type::getInt8Ty(ctx_);
    case 2:  return llvm::Type::getInt16Ty(ctx_);
    case 4:  return llvm::Type::getInt32Ty(ctx_);
    case 8:  return llvm::Type::getInt64Ty(ctx_);
    default: return llvm::Type::getInt16Ty(ctx_);
    }
}

llvm::Type* Codegen::llvmType(const TypeSpec& t) {
    switch (t.base) {
    case BaseType::INTEGER:   return llvmIntType(t.effectiveWidth());
    case BaseType::REAL:
        return (t.effectiveWidth() >= 8) ? llvm::Type::getDoubleTy(ctx_)
                                         : llvm::Type::getFloatTy(ctx_);
    case BaseType::BIT:       return llvmIntType(t.effectiveWidth());
    case BaseType::CHARACTER:
        // CHARACTER*N is represented as [N x i8]
        return llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx_), t.strLen > 0 ? t.strLen : 1);
    case BaseType::VOID:      return llvm::Type::getVoidTy(ctx_);
    default:                  return llvm::Type::getInt16Ty(ctx_);
    }
}

llvm::AllocaInst* Codegen::createEntryAlloca(llvm::Function* f,
                                              const std::string& name,
                                              llvm::Type* ty) {
    llvm::IRBuilder<> entryBuilder(&f->getEntryBlock(),
                                    f->getEntryBlock().begin());
    return entryBuilder.CreateAlloca(ty, nullptr, name);
}

llvm::FunctionCallee Codegen::getPrintf() {
    // Declare printf as: i32 @printf(ptr nocapture readonly, ...)
    if (auto* existing = module_->getFunction("printf"))
        return existing;
    auto* printfTy = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx_),
        { llvm::PointerType::getUnqual(ctx_) },
        /* isVarArg= */ true);
    return module_->getOrInsertFunction("printf", printfTy);
}

llvm::GlobalVariable* Codegen::getRegGlobal(int regNum) {
    if (regGlobals_[regNum]) return regGlobals_[regNum];
    std::string name = "R" + std::to_string(regNum);
    auto* gv = new llvm::GlobalVariable(
        *module_, llvm::Type::getInt16Ty(ctx_), false,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx_), 0), name);
    regGlobals_[regNum] = gv;
    return gv;
}

// ============================================================
// Top-level
// ============================================================

void Codegen::generate(ProgramNode& prog) {
    // Create a top-level "main" function to wrap the program block
    auto* mainFnTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx_), false);
    auto* mainFn   = llvm::Function::Create(mainFnTy, llvm::GlobalValue::ExternalLinkage,
                                             "main", module_.get());
    auto* entry    = llvm::BasicBlock::Create(ctx_, "entry", mainFn);
    builder_.SetInsertPoint(entry);

    curFunc_ = mainFn;
    pushScope();
    genBlock(*prog.block);
    popScope();

    // If no terminator yet, return 0
    if (!builder_.GetInsertBlock()->getTerminator())
        builder_.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0));

    // Verify
    std::string err;
    llvm::raw_string_ostream errStream(err);
    if (llvm::verifyModule(*module_, &errStream)) {
        throw CodegenError("IR verification failed:\n" + errStream.str());
    }
}

// ============================================================
// Block generation
// ============================================================

void Codegen::genBlock(BlockNode& block) {
    pushScope();
    for (auto& d : block.decls) genDecl(*d);
    for (auto& s : block.stmts) genStmt(*s);
    popScope();
}

// ============================================================
// Declaration generation
// ============================================================

void Codegen::genDecl(ASTNode& node) {
    if (auto* vd = dynamic_cast<VarDeclNode*>(&node)) {
        llvm::Type* ty = llvmType(vd->type);

        if (!vd->dims.empty()) {
            // Array: allocate [dims...] x elementType
            llvm::Type* elemTy = ty;
            // Compute total element count for multi-dimensional arrays
            int total = 1;
            for (int d : vd->dims) total *= d;
            ty = llvm::ArrayType::get(elemTy, total);
        }

        llvm::AllocaInst* alloca = createEntryAlloca(curFunc_, vd->name, ty);
        CodegenSymbol sym;
        sym.type   = vd->type;
        sym.kind   = SymbolKind::VARIABLE;
        sym.alloca = alloca;
        setSymbol(vd->name, sym);

    } else if (auto* cd = dynamic_cast<ConstDeclNode*>(&node)) {
        // Constants: store as alloca with initial value (or as LLVM constant)
        llvm::Value* val = genExpr(*cd->value);
        llvm::Type*  ty  = llvmType(cd->type);
        llvm::AllocaInst* alloca = createEntryAlloca(curFunc_, cd->name, ty);
        builder_.CreateStore(val, alloca);
        CodegenSymbol sym;
        sym.type   = cd->type;
        sym.kind   = SymbolKind::CONSTANT;
        sym.alloca = alloca;
        setSymbol(cd->name, sym);

    } else if (auto* pd = dynamic_cast<ProcDeclNode*>(&node)) {
        if (pd->isForward) return;  // forward decl — body comes later

        // Build LLVM function type
        std::vector<llvm::Type*> paramTypes;
        for (auto& p : pd->params) {
            llvm::Type* pt = llvmType(p.type);
            if (p.mode == ParamMode::OUT || p.mode == ParamMode::IN_OUT)
                pt = llvm::PointerType::getUnqual(ctx_);  // pass by pointer (opaque)
            paramTypes.push_back(pt);
        }
        llvm::Type* retTy = llvmType(pd->retType);
        auto* fnTy = llvm::FunctionType::get(retTy, paramTypes, false);
        auto* fn   = llvm::Function::Create(fnTy, llvm::GlobalValue::InternalLinkage,
                                             pd->name, module_.get());

        CodegenSymbol sym;
        sym.type  = pd->retType;
        sym.kind  = (pd->retType.base == BaseType::VOID) ? SymbolKind::PROCEDURE
                                                          : SymbolKind::FUNCTION;
        sym.func  = fn;
        setSymbol(pd->name, sym);

        if (!pd->body) return;

        // Generate body
        auto* prevBlock = builder_.GetInsertBlock();
        auto* prevFunc  = curFunc_;

        auto* entry = llvm::BasicBlock::Create(ctx_, "entry", fn);
        builder_.SetInsertPoint(entry);
        curFunc_ = fn;
        labels_.clear();

        pushScope();

        // Bind parameters to allocas
        size_t idx = 0;
        for (auto& arg : fn->args()) {
            const auto& param = pd->params[idx++];
            arg.setName(param.name);
            llvm::AllocaInst* alloca = createEntryAlloca(fn, param.name,
                                                          llvmType(param.type));
            builder_.CreateStore(&arg, alloca);
            CodegenSymbol psym;
            psym.type   = param.type;
            psym.kind   = SymbolKind::PARAMETER;
            psym.alloca = alloca;
            setSymbol(param.name, psym);
        }

        // Generate body decls and stmts
        for (auto& d : pd->body->decls) genDecl(*d);
        for (auto& s : pd->body->stmts) genStmt(*s);

        // Default return if no terminator
        if (!builder_.GetInsertBlock()->getTerminator()) {
            if (pd->retType.base == BaseType::VOID)
                builder_.CreateRetVoid();
            else
                builder_.CreateRet(llvm::Constant::getNullValue(retTy));
        }

        popScope();

        // Restore previous insert point
        curFunc_ = prevFunc;
        if (prevBlock) builder_.SetInsertPoint(prevBlock);
    }
}

// ============================================================
// Statement generation
// ============================================================

void Codegen::genStmt(ASTNode& node) {
    if (auto* b = dynamic_cast<BlockNode*>(&node)) {
        genBlock(*b);

    } else if (auto* a = dynamic_cast<AssignStmtNode*>(&node)) {
        llvm::Value* lhs = genLValue(*a->target);
        llvm::Value* rhs = genExpr(*a->value);
        // Determine lhs element type from the codegen symbol (avoid opaque pointer issues)
        // We use the rhs type as the store type, with coercion when needed.
        // Find element type via stored symbol type.
        llvm::Type* lhsElemTy = nullptr;
        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(lhs))
            lhsElemTy = ai->getAllocatedType();
        else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(lhs))
            lhsElemTy = gv->getValueType();
        else
            lhsElemTy = rhs->getType();  // fallback: trust rhs type

        if (rhs->getType() != lhsElemTy) {
            if (lhsElemTy->isIntegerTy() && rhs->getType()->isIntegerTy())
                rhs = builder_.CreateIntCast(rhs, lhsElemTy, true, "icast");
            else if ((lhsElemTy->isFloatTy() || lhsElemTy->isDoubleTy()) &&
                     (rhs->getType()->isFloatTy() || rhs->getType()->isDoubleTy()))
                rhs = builder_.CreateFPCast(rhs, lhsElemTy, "fpcast");
            else if (lhsElemTy->isFloatTy() && rhs->getType()->isIntegerTy())
                rhs = builder_.CreateSIToFP(rhs, lhsElemTy, "itof");
        }
        builder_.CreateStore(rhs, lhs);

    } else if (auto* is = dynamic_cast<IfStmtNode*>(&node)) {
        llvm::Value* cond = genExpr(*is->cond);
        // Convert to i1
        cond = builder_.CreateICmpNE(cond,
               llvm::ConstantInt::get(cond->getType(), 0), "ifcond");

        auto* thenBB = llvm::BasicBlock::Create(ctx_, "then", curFunc_);
        auto* mergeBB = llvm::BasicBlock::Create(ctx_, "ifcont", curFunc_);
        llvm::BasicBlock* elseBB = mergeBB;
        if (is->elseStmt) elseBB = llvm::BasicBlock::Create(ctx_, "else", curFunc_);

        builder_.CreateCondBr(cond, thenBB, elseBB);

        builder_.SetInsertPoint(thenBB);
        genStmt(*is->thenStmt);
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(mergeBB);

        if (is->elseStmt) {
            builder_.SetInsertPoint(elseBB);
            genStmt(*is->elseStmt);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(mergeBB);
        }

        builder_.SetInsertPoint(mergeBB);

    } else if (auto* ws = dynamic_cast<WhileStmtNode*>(&node)) {
        auto* condBB  = llvm::BasicBlock::Create(ctx_, "while.cond", curFunc_);
        auto* bodyBB  = llvm::BasicBlock::Create(ctx_, "while.body", curFunc_);
        auto* exitBB  = llvm::BasicBlock::Create(ctx_, "while.exit", curFunc_);

        builder_.CreateBr(condBB);
        builder_.SetInsertPoint(condBB);
        llvm::Value* cond = genExpr(*ws->cond);
        cond = builder_.CreateICmpNE(cond,
               llvm::ConstantInt::get(cond->getType(), 0), "whilecond");
        builder_.CreateCondBr(cond, bodyBB, exitBB);

        builder_.SetInsertPoint(bodyBB);
        genStmt(*ws->body);

        if (ws->untilCond) {
            auto* untilBB = llvm::BasicBlock::Create(ctx_, "while.until", curFunc_);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(untilBB);
            builder_.SetInsertPoint(untilBB);
            llvm::Value* uc = genExpr(*ws->untilCond);
            uc = builder_.CreateICmpNE(uc,
                 llvm::ConstantInt::get(uc->getType(), 0), "untilcond");
            builder_.CreateCondBr(uc, exitBB, condBB);
        } else {
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(condBB);
        }

        builder_.SetInsertPoint(exitBB);

    } else if (auto* fs = dynamic_cast<ForStmtNode*>(&node)) {
        // FOR i := start TO/DOWNTO end DO body
        // FOR i FROM start [STEP expr] TO/DOWNTO end DO body
        CodegenSymbol* sym = lookupSymbol(fs->loopVar);
        if (!sym) throw CodegenError("Undefined loop variable: " + fs->loopVar);

        llvm::Type*  iType  = sym->alloca->getAllocatedType();
        llvm::Value* endVal = genExpr(*fs->end);

        // Step: use explicit STEP expression, or default to ±1 from TO/DOWNTO direction
        llvm::Value* stepVal;
        if (fs->step) {
            stepVal = genExpr(*fs->step);
            stepVal = builder_.CreateIntCast(stepVal, iType, true, "step");
        } else {
            stepVal = llvm::ConstantInt::getSigned(iType, fs->downto ? -1 : 1);
        }

        // Initialise loop variable only when a start value was given.
        // When omitted, the loop begins at whatever value the variable already holds.
        if (fs->start) {
            llvm::Value* startVal = genExpr(*fs->start);
            builder_.CreateStore(startVal, sym->alloca);
        }

        auto* condBB = llvm::BasicBlock::Create(ctx_, "for.cond", curFunc_);
        auto* bodyBB = llvm::BasicBlock::Create(ctx_, "for.body", curFunc_);
        auto* exitBB = llvm::BasicBlock::Create(ctx_, "for.exit", curFunc_);

        builder_.CreateBr(condBB);
        builder_.SetInsertPoint(condBB);

        llvm::Value* cur = builder_.CreateLoad(sym->alloca->getAllocatedType(),
                                                sym->alloca, fs->loopVar);
        llvm::Value* cond;
        if (fs->downto)
            cond = builder_.CreateICmpSGE(cur, endVal, "forcond");
        else
            cond = builder_.CreateICmpSLE(cur, endVal, "forcond");
        builder_.CreateCondBr(cond, bodyBB, exitBB);

        builder_.SetInsertPoint(bodyBB);
        genStmt(*fs->body);

        if (fs->untilCond) {
            // UNTIL check sits between body and the increment step
            auto* untilBB = llvm::BasicBlock::Create(ctx_, "for.until", curFunc_);
            auto* stepBB  = llvm::BasicBlock::Create(ctx_, "for.step",  curFunc_);

            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(untilBB);
            builder_.SetInsertPoint(untilBB);
            llvm::Value* uc = genExpr(*fs->untilCond);
            uc = builder_.CreateICmpNE(uc,
                 llvm::ConstantInt::get(uc->getType(), 0), "untilcond");
            builder_.CreateCondBr(uc, exitBB, stepBB);

            builder_.SetInsertPoint(stepBB);
            llvm::Value* next = builder_.CreateAdd(
                builder_.CreateLoad(iType, sym->alloca, "forvar"),
                stepVal, "forstep");
            builder_.CreateStore(next, sym->alloca);
            builder_.CreateBr(condBB);
        } else {
            llvm::Value* next = builder_.CreateAdd(
                builder_.CreateLoad(iType, sym->alloca, "forvar"),
                stepVal, "forstep");
            builder_.CreateStore(next, sym->alloca);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(condBB);
        }

        builder_.SetInsertPoint(exitBB);

    } else if (auto* ds = dynamic_cast<DoStmtNode*>(&node)) {
        auto* bodyBB = llvm::BasicBlock::Create(ctx_, "do.body", curFunc_);
        builder_.CreateBr(bodyBB);
        builder_.SetInsertPoint(bodyBB);
        if (ds->body) genStmt(*ds->body);

        if (ds->untilCond) {
            auto* untilBB = llvm::BasicBlock::Create(ctx_, "do.until", curFunc_);
            auto* exitBB  = llvm::BasicBlock::Create(ctx_, "do.exit",  curFunc_);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(untilBB);
            builder_.SetInsertPoint(untilBB);
            llvm::Value* uc = genExpr(*ds->untilCond);
            uc = builder_.CreateICmpNE(uc,
                 llvm::ConstantInt::get(uc->getType(), 0), "docond");
            builder_.CreateCondBr(uc, exitBB, bodyBB);
            builder_.SetInsertPoint(exitBB);
        } else {
            // No UNTIL — infinite loop; back-edge to do.body
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(bodyBB);
            // Subsequent code is unreachable; give it a home
            auto* deadBB = llvm::BasicBlock::Create(ctx_, "do.after", curFunc_);
            builder_.SetInsertPoint(deadBB);
        }

    } else if (auto* rs = dynamic_cast<RepeatStmtNode*>(&node)) {
        auto* bodyBB = llvm::BasicBlock::Create(ctx_, "repeat.body", curFunc_);
        auto* exitBB = llvm::BasicBlock::Create(ctx_, "repeat.exit", curFunc_);

        builder_.CreateBr(bodyBB);
        builder_.SetInsertPoint(bodyBB);
        for (auto& s : rs->stmts) genStmt(*s);
        llvm::Value* cond = genExpr(*rs->cond);
        cond = builder_.CreateICmpNE(cond,
               llvm::ConstantInt::get(cond->getType(), 0), "repeatcond");
        builder_.CreateCondBr(cond, exitBB, bodyBB);
        builder_.SetInsertPoint(exitBB);

    } else if (auto* cs = dynamic_cast<CaseStmtNode*>(&node)) {
        llvm::Value* expr = genExpr(*cs->expr);
        auto* defaultBB   = llvm::BasicBlock::Create(ctx_, "case.default", curFunc_);
        auto* mergeBB     = llvm::BasicBlock::Create(ctx_, "case.end",     curFunc_);
        auto* sw          = builder_.CreateSwitch(expr, defaultBB,
                                                   static_cast<unsigned>(cs->arms.size()));

        for (auto& armNode : cs->arms) {
            auto* arm   = dynamic_cast<CaseArmNode*>(armNode.get());
            auto* armBB = llvm::BasicBlock::Create(ctx_, "case.arm", curFunc_);
            sw->addCase(llvm::ConstantInt::get(
                            llvm::cast<llvm::IntegerType>(expr->getType()), arm->value),
                        armBB);
            builder_.SetInsertPoint(armBB);
            genStmt(*arm->stmt);
            if (!builder_.GetInsertBlock()->getTerminator())
                builder_.CreateBr(mergeBB);
        }

        builder_.SetInsertPoint(defaultBB);
        builder_.CreateBr(mergeBB);
        builder_.SetInsertPoint(mergeBB);

    } else if (auto* pc = dynamic_cast<ProcCallStmtNode*>(&node)) {
        CodegenSymbol* sym = lookupSymbol(pc->name);
        if (!sym || !sym->func)
            throw CodegenError("Undefined procedure: " + pc->name);
        std::vector<llvm::Value*> args;
        for (auto& a : pc->args) args.push_back(genExpr(*a));
        builder_.CreateCall(sym->func, args);

    } else if (auto* ret = dynamic_cast<ReturnStmtNode*>(&node)) {
        if (ret->value) {
            llvm::Value* val = genExpr(*ret->value);
            builder_.CreateRet(val);
        } else {
            builder_.CreateRetVoid();
        }
        // Create an unreachable block to absorb any subsequent stmts
        auto* deadBB = llvm::BasicBlock::Create(ctx_, "unreachable", curFunc_);
        builder_.SetInsertPoint(deadBB);

    } else if (auto* gs = dynamic_cast<GotoStmtNode*>(&node)) {
        auto it = labels_.find(gs->label);
        if (it == labels_.end()) {
            // Create forward label block
            labels_[gs->label] = llvm::BasicBlock::Create(ctx_, "label." + gs->label, curFunc_);
        }
        builder_.CreateBr(labels_[gs->label]);
        auto* deadBB = llvm::BasicBlock::Create(ctx_, "aftergoto", curFunc_);
        builder_.SetInsertPoint(deadBB);

    } else if (auto* ls = dynamic_cast<LabelStmtNode*>(&node)) {
        auto it = labels_.find(ls->label);
        llvm::BasicBlock* labelBB;
        if (it == labels_.end()) {
            labelBB = llvm::BasicBlock::Create(ctx_, "label." + ls->label, curFunc_);
            labels_[ls->label] = labelBB;
        } else {
            labelBB = it->second;
        }
        if (!builder_.GetInsertBlock()->getTerminator())
            builder_.CreateBr(labelBB);
        builder_.SetInsertPoint(labelBB);
        genStmt(*ls->stmt);

    } else if (auto* ps = dynamic_cast<PrintStmtNode*>(&node)) {
        // Build a C-compatible format string:
        //   %d → %d  (WORD/BYTE, sign-extended to i32)
        //   %l → %d  (LONG is i32; use plain %d for C printf)
        //   %f → %f  (REAL, promoted to double for varargs)
        //   %c → %c  (integer, promoted to i32)
        //   %s → %s  (CHARACTER pointer)
        //   %% → %%  (literal percent)
        std::string cFmt;
        cFmt.reserve(ps->fmt.size() + 1);
        for (size_t i = 0; i < ps->fmt.size(); ++i) {
            if (ps->fmt[i] == '%' && i + 1 < ps->fmt.size()) {
                char spec = ps->fmt[i + 1];
                cFmt += '%';
                cFmt += (spec == 'l') ? 'd' : spec;  // %l → %d
                ++i;
            } else {
                cFmt += ps->fmt[i];
            }
        }
        cFmt += '\0';

        // Create a global constant for the format string
        llvm::Constant* fmtConst = llvm::ConstantDataArray::getString(ctx_, cFmt, false);
        auto* fmtGV = new llvm::GlobalVariable(
            *module_, fmtConst->getType(), true,
            llvm::GlobalValue::PrivateLinkage, fmtConst, ".fmt");
        fmtGV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        fmtGV->setAlignment(llvm::Align(1));

        llvm::Value* fmtPtr = builder_.CreatePointerCast(
            fmtGV, llvm::PointerType::getUnqual(ctx_), "fmtptr");

        // Build the argument list, applying required promotions for printf varargs
        std::vector<llvm::Value*> callArgs = { fmtPtr };
        for (size_t i = 0; i < ps->args.size(); ++i) {
            llvm::Value* v = genExpr(*ps->args[i]);
            switch (ps->specs[i]) {
            case FmtSpec::D:
            case FmtSpec::C:
            case FmtSpec::L:
                // Promote to i32 (printf vararg requirement)
                if (v->getType()->isIntegerTy() &&
                    v->getType()->getIntegerBitWidth() < 32)
                    v = builder_.CreateSExt(v, llvm::Type::getInt32Ty(ctx_), "iext");
                break;
            case FmtSpec::F:
                // Promote to double (printf vararg requirement)
                if (v->getType()->isFloatTy())
                    v = builder_.CreateFPExt(v, llvm::Type::getDoubleTy(ctx_), "fpext");
                else if (v->getType()->isIntegerTy())
                    v = builder_.CreateSIToFP(v, llvm::Type::getDoubleTy(ctx_), "itof");
                break;
            case FmtSpec::S:
                // CHARACTER arrays: get pointer to first element via GEP.
                // Re-generate as lvalue to get the alloca, then index [0,0].
                if (v->getType()->isArrayTy()) {
                    llvm::Value* ptr = genLValue(*ps->args[i]);
                    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0);
                    // Determine the alloca's allocated type to form the correct GEP
                    llvm::Type* allocTy = llvm::cast<llvm::AllocaInst>(ptr)->getAllocatedType();
                    v = builder_.CreateGEP(allocTy, ptr, { zero, zero }, "strptr");
                }
                break;
            }
            callArgs.push_back(v);
        }

        builder_.CreateCall(getPrintf(), callArgs);

    } else if (auto* as = dynamic_cast<AsmStmtNode*>(&node)) {
        // Emit inline asm — no operands, no constraints (educational stub)
        llvm::FunctionType* asmTy = llvm::FunctionType::get(
            llvm::Type::getVoidTy(ctx_), false);
        // Warn: inline asm is PDP-11 specific
        auto* ia = llvm::InlineAsm::get(asmTy, as->asmText, "",
                                         true  /* hasSideEffects */);
        builder_.CreateCall(ia);

    } else if (auto* push = dynamic_cast<PushStmtNode*>(&node)) {
        // PUSH expr — simulated PDP-11 pre-decrement stack push.
        // Uses a 256-word global array (__pl11_stack) and an index (__pl11_sp_idx).
        // R6 (SP) is kept synchronised as the simulated 16-bit address (idx * 2).
        ensureSimStack();
        llvm::Type* i16Ty = llvm::Type::getInt16Ty(ctx_);
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(ctx_);

        // Pre-decrement index
        llvm::Value* idxOld = builder_.CreateLoad(i32Ty, simStackIdx_, "sp.idx");
        llvm::Value* idxNew = builder_.CreateSub(
            idxOld, llvm::ConstantInt::get(i32Ty, 1), "sp.idx.push");
        builder_.CreateStore(idxNew, simStackIdx_);

        // Update R6 (simulated SP address = index * 2)
        llvm::Value* spAddr = builder_.CreateMul(
            idxNew, llvm::ConstantInt::get(i32Ty, 2), "sp.addr");
        llvm::Value* spAddr16 = builder_.CreateTrunc(spAddr, i16Ty, "sp.i16");
        builder_.CreateStore(spAddr16, getRegGlobal(6));

        // Store value into stack[idxNew]
        llvm::Value* val = genExpr(*push->value);
        val = builder_.CreateIntCast(val, i16Ty, true, "push.val");
        llvm::Value* zero = llvm::ConstantInt::get(i32Ty, 0);
        llvm::Value* gep = builder_.CreateGEP(
            simStack_->getValueType(), simStack_,
            {zero, idxNew}, "push.slot");
        builder_.CreateStore(val, gep);

    } else if (auto* pop = dynamic_cast<PopStmtNode*>(&node)) {
        // POP target — simulated PDP-11 post-increment stack pop.
        ensureSimStack();
        llvm::Type* i16Ty = llvm::Type::getInt16Ty(ctx_);
        llvm::Type* i32Ty = llvm::Type::getInt32Ty(ctx_);

        // Load value from stack[idx]
        llvm::Value* idxOld = builder_.CreateLoad(i32Ty, simStackIdx_, "sp.idx");
        llvm::Value* zero   = llvm::ConstantInt::get(i32Ty, 0);
        llvm::Value* gep    = builder_.CreateGEP(
            simStack_->getValueType(), simStack_,
            {zero, idxOld}, "pop.slot");
        llvm::Value* val = builder_.CreateLoad(i16Ty, gep, "pop.val");

        // Store into target
        llvm::Value* targetPtr = genLValue(*pop->target);
        builder_.CreateStore(val, targetPtr);

        // Post-increment index
        llvm::Value* idxNew = builder_.CreateAdd(
            idxOld, llvm::ConstantInt::get(i32Ty, 1), "sp.idx.pop");
        builder_.CreateStore(idxNew, simStackIdx_);

        // Update R6 (simulated SP address = index * 2)
        llvm::Value* spAddr = builder_.CreateMul(
            idxNew, llvm::ConstantInt::get(i32Ty, 2), "sp.addr");
        llvm::Value* spAddr16 = builder_.CreateTrunc(spAddr, i16Ty, "sp.i16");
        builder_.CreateStore(spAddr16, getRegGlobal(6));
    }
}

// ============================================================
// Simulated PDP-11 stack helpers
// ============================================================

void Codegen::ensureSimStack() {
    if (simStack_) return;
    llvm::Type* i16Ty  = llvm::Type::getInt16Ty(ctx_);
    llvm::Type* i32Ty  = llvm::Type::getInt32Ty(ctx_);
    auto* stackArrTy   = llvm::ArrayType::get(i16Ty, 256);

    // [256 x i16] zero-initialized global array
    simStack_ = new llvm::GlobalVariable(
        *module_, stackArrTy, false,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantAggregateZero::get(stackArrTy),
        "__pl11_stack");

    // i32 index, initialised to 256 (stack empty — grows downward)
    simStackIdx_ = new llvm::GlobalVariable(
        *module_, i32Ty, false,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantInt::get(i32Ty, 256),
        "__pl11_sp_idx");
}

// ============================================================
// Expression generation
// ============================================================

llvm::Value* Codegen::genExpr(ASTNode& node) {
    if (auto* n = dynamic_cast<IntLiteralNode*>(&node)) {
        // Use the narrowest signed type that holds the value without truncation.
        // Assignment codegen will widen/narrow to the target variable's type.
        long long v = n->value;
        if (v >= -32768 && v <= 32767)
            return llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx_), v, true);
        if (v >= -2147483648LL && v <= 2147483647LL)
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), v, true);
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx_), v, true);
    }

    if (auto* n = dynamic_cast<RealLiteralNode*>(&node))
        return llvm::ConstantFP::get(llvm::Type::getFloatTy(ctx_), n->value);

    if (auto* n = dynamic_cast<BitLiteralNode*>(&node))
        return llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx_), n->intValue);

    if (auto* n = dynamic_cast<AddressLiteralNode*>(&node))
        return llvm::ConstantInt::get(llvm::Type::getInt16Ty(ctx_), n->address);

    if (auto* n = dynamic_cast<StringLiteralNode*>(&node))
        return builder_.CreateGlobalStringPtr(n->value);

    if (auto* rn = dynamic_cast<RegisterNode*>(&node)) {
        llvm::errs() << "Warning: Register access (R" << rn->regNum
                     << ") is PDP-11 specific — emitting global variable access\n";
        auto* gv = getRegGlobal(rn->regNum);
        return builder_.CreateLoad(llvm::Type::getInt16Ty(ctx_), gv, "R" + std::to_string(rn->regNum));
    }

    if (auto* n = dynamic_cast<IdentifierNode*>(&node)) {
        CodegenSymbol* sym = lookupSymbol(n->name);
        if (!sym) throw CodegenError("Undefined: " + n->name, n->loc);
        if (sym->alloca)
            return builder_.CreateLoad(sym->alloca->getAllocatedType(), sym->alloca, n->name);
        if (sym->global)
            return builder_.CreateLoad(sym->global->getValueType(), sym->global, n->name);
        throw CodegenError("Cannot load value of '" + n->name + "'", n->loc);
    }

    if (auto* n = dynamic_cast<UnaryExprNode*>(&node)) {
        llvm::Value* val = genExpr(*n->operand);
        switch (n->op) {
        case TokenKind::TOK_MINUS:
            if (val->getType()->isFloatingPointTy())
                return builder_.CreateFNeg(val, "fneg");
            return builder_.CreateNeg(val, "neg");
        case TokenKind::TOK_NOT:
            return builder_.CreateNot(val, "not");
        default:
            throw CodegenError("Unknown unary op");
        }
    }

    if (auto* n = dynamic_cast<BinaryExprNode*>(&node)) {
        llvm::Value* l = genExpr(*n->left);
        llvm::Value* r = genExpr(*n->right);
        bool isFloat = l->getType()->isFloatingPointTy() || r->getType()->isFloatingPointTy();

        // Promote integers to same width
        if (!isFloat && l->getType() != r->getType()) {
            auto* wider = (l->getType()->getIntegerBitWidth() >
                           r->getType()->getIntegerBitWidth()) ? l->getType() : r->getType();
            l = builder_.CreateSExt(l, wider, "lext");
            r = builder_.CreateSExt(r, wider, "rext");
        }

        switch (n->op) {
        case TokenKind::TOK_PLUS:
            return isFloat ? builder_.CreateFAdd(l, r, "fadd") : builder_.CreateAdd(l, r, "add");
        case TokenKind::TOK_MINUS:
            return isFloat ? builder_.CreateFSub(l, r, "fsub") : builder_.CreateSub(l, r, "sub");
        case TokenKind::TOK_STAR:
            return isFloat ? builder_.CreateFMul(l, r, "fmul") : builder_.CreateMul(l, r, "mul");
        case TokenKind::TOK_SLASH:
            return isFloat ? builder_.CreateFDiv(l, r, "fdiv") : builder_.CreateSDiv(l, r, "div");
        case TokenKind::TOK_MOD:
            return builder_.CreateSRem(l, r, "mod");
        case TokenKind::TOK_AND:
            return builder_.CreateAnd(l, r, "and");
        case TokenKind::TOK_OR:
            return builder_.CreateOr(l, r, "or");
        case TokenKind::TOK_XOR:
            return builder_.CreateXor(l, r, "xor");
        case TokenKind::TOK_SHL:
            return builder_.CreateShl(l, r, "shl");
        case TokenKind::TOK_SHR:
            return builder_.CreateLShr(l, r, "shr");
        case TokenKind::TOK_SHRA:
            return builder_.CreateAShr(l, r, "shra");
        // Relational — return i16 (0 or 1)
        case TokenKind::TOK_EQ:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpOEQ(l, r, "feq")
                        : builder_.CreateICmpEQ(l, r, "eq"),
                llvm::Type::getInt16Ty(ctx_), false);
        case TokenKind::TOK_NEQ:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpONE(l, r, "fne")
                        : builder_.CreateICmpNE(l, r, "ne"),
                llvm::Type::getInt16Ty(ctx_), false);
        case TokenKind::TOK_LT:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpOLT(l, r, "flt")
                        : builder_.CreateICmpSLT(l, r, "lt"),
                llvm::Type::getInt16Ty(ctx_), false);
        case TokenKind::TOK_GT:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpOGT(l, r, "fgt")
                        : builder_.CreateICmpSGT(l, r, "gt"),
                llvm::Type::getInt16Ty(ctx_), false);
        case TokenKind::TOK_LEQ:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpOLE(l, r, "fle")
                        : builder_.CreateICmpSLE(l, r, "le"),
                llvm::Type::getInt16Ty(ctx_), false);
        case TokenKind::TOK_GEQ:
            return builder_.CreateIntCast(
                isFloat ? builder_.CreateFCmpOGE(l, r, "fge")
                        : builder_.CreateICmpSGE(l, r, "ge"),
                llvm::Type::getInt16Ty(ctx_), false);
        default:
            throw CodegenError("Unknown binary operator");
        }
    }

    if (auto* n = dynamic_cast<DerefExprNode*>(&node)) {
        llvm::Value* ptr = genExpr(*n->ptr);
        // Cast integer to pointer then load
        auto* ptrTy = llvm::PointerType::getUnqual(ctx_);
        ptr = builder_.CreateIntToPtr(ptr, ptrTy, "deref_ptr");
        return builder_.CreateLoad(llvm::Type::getInt16Ty(ctx_), ptr, "deref");
    }

    if (auto* n = dynamic_cast<AddrOfExprNode*>(&node)) {
        llvm::Value* lval = genLValue(*n->var);
        return builder_.CreatePtrToInt(lval, llvm::Type::getInt16Ty(ctx_), "addr");
    }

    if (auto* n = dynamic_cast<IndexExprNode*>(&node)) {
        // Array element access: get pointer to element, then load
        llvm::Value* ptr = genLValue(*n);
        // Determine element type from the base identifier's symbol
        llvm::Type* elemTy = llvm::Type::getInt16Ty(ctx_);
        if (auto* id = dynamic_cast<IdentifierNode*>(n->base.get())) {
            CodegenSymbol* sym = lookupSymbol(id->name);
            if (sym) elemTy = llvmType(sym->type);
        }
        return builder_.CreateLoad(elemTy, ptr, "elem");
    }

    if (auto* n = dynamic_cast<FuncCallExprNode*>(&node)) {
        CodegenSymbol* sym = lookupSymbol(n->name);
        if (!sym) throw CodegenError("Undefined: " + n->name, n->loc);

        // Array read — same disambiguation as sema: VARIABLE/CONSTANT → array index
        if (sym->kind == SymbolKind::VARIABLE || sym->kind == SymbolKind::CONSTANT) {
            if (!sym->alloca)
                throw CodegenError("Cannot index non-array: " + n->name, n->loc);
            std::vector<llvm::Value*> idxs;
            idxs.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0));
            for (auto& arg : n->args) {
                llvm::Value* i = genExpr(*arg);
                i = builder_.CreateIntCast(i, llvm::Type::getInt32Ty(ctx_), true);
                i = builder_.CreateSub(i,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 1), "idx0");
                idxs.push_back(i);
            }
            llvm::Value* ptr = builder_.CreateGEP(
                sym->alloca->getAllocatedType(), sym->alloca, idxs, "elemptr");
            return builder_.CreateLoad(llvmType(sym->type), ptr, "elem");
        }

        if (!sym->func)
            throw CodegenError("Undefined function: " + n->name, n->loc);
        std::vector<llvm::Value*> args;
        for (auto& a : n->args) args.push_back(genExpr(*a));
        return builder_.CreateCall(sym->func, args, "call");
    }

    throw CodegenError("Cannot generate expression for node kind " +
                       std::to_string(static_cast<int>(node.kind)));
}

// ============================================================
// LValue generation (returns pointer to storage)
// ============================================================

llvm::Value* Codegen::genLValue(ASTNode& node) {
    if (auto* n = dynamic_cast<IdentifierNode*>(&node)) {
        CodegenSymbol* sym = lookupSymbol(n->name);
        if (!sym) throw CodegenError("Undefined: " + n->name, n->loc);
        if (sym->alloca) return sym->alloca;
        if (sym->global) return sym->global;
        throw CodegenError("Not addressable: " + n->name, n->loc);
    }

    if (auto* n = dynamic_cast<RegisterNode*>(&node)) {
        return getRegGlobal(n->regNum);
    }

    if (auto* n = dynamic_cast<DerefExprNode*>(&node)) {
        llvm::Value* ptrVal = genExpr(*n->ptr);
        auto* ptrTy = llvm::PointerType::getUnqual(ctx_);
        return builder_.CreateIntToPtr(ptrVal, ptrTy, "deref_lval");
    }

    if (auto* n = dynamic_cast<IndexExprNode*>(&node)) {
        // Get base alloca
        CodegenSymbol* sym = nullptr;
        if (auto* id = dynamic_cast<IdentifierNode*>(n->base.get()))
            sym = lookupSymbol(id->name);
        if (!sym || !sym->alloca)
            throw CodegenError("Cannot index non-array");

        // Build GEP: [0, index0, index1, ...]
        std::vector<llvm::Value*> idxs;
        idxs.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 0));
        for (auto& idx : n->indices) {
            llvm::Value* i = genExpr(*idx);
            // Convert to i32 for GEP; subtract 1 because PL-11 arrays are 1-based
            i = builder_.CreateIntCast(i, llvm::Type::getInt32Ty(ctx_), true);
            i = builder_.CreateSub(i, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx_), 1),
                                   "idx0");
            idxs.push_back(i);
        }
        return builder_.CreateGEP(sym->alloca->getAllocatedType(), sym->alloca, idxs, "elemptr");
    }

    throw CodegenError("Expression is not an lvalue");
}

void Codegen::emitIR() {
    module_->print(llvm::outs(), nullptr);
}

} // namespace pl11
