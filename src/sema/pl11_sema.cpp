#include "pl11_sema.h"
#include <sstream>

namespace pl11 {

// ============================================================
// Scope
// ============================================================

void Scope::declare(const std::string& name, Symbol sym) {
    symbols_[name] = std::move(sym);
}

Symbol* Scope::lookup(const std::string& name) {
    auto it = symbols_.find(name);
    return it != symbols_.end() ? &it->second : nullptr;
}

bool Scope::localLookup(const std::string& name) const {
    return symbols_.count(name) > 0;
}

// ============================================================
// Sema
// ============================================================

Sema::Sema() {
    curReturnType_.base = BaseType::VOID;
}

void Sema::pushScope() {
    scopes_.push_back(std::make_unique<Scope>());
}

void Sema::popScope() {
    scopes_.pop_back();
}

void Sema::declare(const std::string& name, Symbol sym, SourceLoc loc) {
    auto& scope = *scopes_.back();
    if (scope.localLookup(name)) {
        throw SemanticError("Redeclaration of '" + name + "'", loc);
    }
    scope.declare(name, std::move(sym));
}

Symbol* Sema::lookup(const std::string& name, SourceLoc loc) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        if (auto* sym = scopes_[i]->lookup(name))
            return sym;
    }
    throw SemanticError("Undefined identifier '" + name + "'", loc);
}

SemanticError Sema::typeError(const std::string& msg, SourceLoc loc) {
    return SemanticError(loc.file + ":" + std::to_string(loc.line) + ": type error: " + msg, loc);
}

// ============================================================
// Type helpers
// ============================================================

bool Sema::isArithmetic(const TypeSpec& t) {
    return t.base == BaseType::INTEGER || t.base == BaseType::REAL;
}

bool Sema::compatible(const TypeSpec& a, const TypeSpec& b) {
    if (a.base == b.base) return true;
    // INTEGER and BIT are compatible (bitwise ops on integers)
    if ((a.base == BaseType::INTEGER && b.base == BaseType::BIT) ||
        (a.base == BaseType::BIT    && b.base == BaseType::INTEGER)) return true;
    // INTEGER and REAL are compatible (implicit conversion)
    if (isArithmetic(a) && isArithmetic(b)) return true;
    return false;
}

TypeSpec Sema::promoteArith(const TypeSpec& a, const TypeSpec& b) {
    // REAL promotes INTEGER
    if (a.base == BaseType::REAL || b.base == BaseType::REAL) {
        TypeSpec r; r.base = BaseType::REAL; r.width = std::max(a.width, b.width); return r;
    }
    TypeSpec r; r.base = BaseType::INTEGER;
    r.width = std::max(a.effectiveWidth(), b.effectiveWidth());
    return r;
}

// ============================================================
// Analysis entry point
// ============================================================

void Sema::analyse(ProgramNode& prog) {
    pushScope();
    analyseBlock(*prog.block);
    popScope();
}

void Sema::analyseBlock(BlockNode& block) {
    pushScope();
    for (auto& d : block.decls) analyseDecl(*d);
    for (auto& s : block.stmts) analyseStmt(*s);
    popScope();
}

// ============================================================
// Declaration analysis
// ============================================================

void Sema::analyseDecl(ASTNode& node) {
    if (auto* vd = dynamic_cast<VarDeclNode*>(&node)) {
        Symbol sym;
        sym.name = vd->name;
        sym.kind = SymbolKind::VARIABLE;
        sym.type = vd->type;
        declare(vd->name, sym, vd->loc);
    } else if (auto* cd = dynamic_cast<ConstDeclNode*>(&node)) {
        TypeSpec valType = analyseExpr(*cd->value);
        if (!compatible(cd->type, valType))
            throw typeError("Constant initializer type mismatch", cd->loc);
        Symbol sym;
        sym.name = cd->name;
        sym.kind = SymbolKind::CONSTANT;
        sym.type = cd->type;
        declare(cd->name, sym, cd->loc);
    } else if (auto* pd = dynamic_cast<ProcDeclNode*>(&node)) {
        // Register the procedure in the current scope before analysing body
        // (allows recursion)
        Symbol sym;
        sym.name    = pd->name;
        sym.kind    = (pd->retType.base == BaseType::VOID)
                      ? SymbolKind::PROCEDURE : SymbolKind::FUNCTION;
        sym.type    = pd->retType;
        sym.params  = pd->params;
        sym.retType = pd->retType;
        declare(pd->name, sym, pd->loc);

        if (!pd->isForward && pd->body) {
            // Create scope for parameters + body
            pushScope();
            for (auto& p : pd->params) {
                Symbol psym;
                psym.name = p.name;
                psym.kind = SymbolKind::PARAMETER;
                psym.type = p.type;
                declare(p.name, psym, pd->loc);
            }
            TypeSpec savedRet = curReturnType_;
            curReturnType_ = pd->retType;
            // Analyse body declarations + statements directly (don't double-push scope)
            for (auto& d : pd->body->decls) analyseDecl(*d);
            for (auto& s : pd->body->stmts) analyseStmt(*s);
            curReturnType_ = savedRet;
            popScope();
        }
    }
}

// ============================================================
// Statement analysis
// ============================================================

void Sema::analyseStmt(ASTNode& node) {
    if (auto* b = dynamic_cast<BlockNode*>(&node)) {
        analyseBlock(*b);
    } else if (auto* a = dynamic_cast<AssignStmtNode*>(&node)) {
        TypeSpec lhsType = analyseExpr(*a->target);
        TypeSpec rhsType = analyseExpr(*a->value);
        if (!compatible(lhsType, rhsType))
            throw typeError("Type mismatch in assignment", a->loc);
    } else if (auto* is = dynamic_cast<IfStmtNode*>(&node)) {
        analyseExpr(*is->cond);
        analyseStmt(*is->thenStmt);
        if (is->elseStmt) analyseStmt(*is->elseStmt);
    } else if (auto* ws = dynamic_cast<WhileStmtNode*>(&node)) {
        analyseExpr(*ws->cond);
        analyseStmt(*ws->body);
        if (ws->untilCond) analyseExpr(*ws->untilCond);
    } else if (auto* fs = dynamic_cast<ForStmtNode*>(&node)) {
        Symbol* sym = lookup(fs->loopVar, fs->loc);
        if (sym->type.base != BaseType::INTEGER)
            throw typeError("FOR loop variable must be INTEGER", fs->loc);
        if (fs->start) {
            TypeSpec st = analyseExpr(*fs->start);
            if (!isArithmetic(st))
                throw typeError("FOR loop start value must be arithmetic", fs->loc);
        }
        TypeSpec et = analyseExpr(*fs->end);
        if (!isArithmetic(et))
            throw typeError("FOR loop end value must be arithmetic", fs->loc);
        if (fs->step) {
            TypeSpec stepT = analyseExpr(*fs->step);
            if (!isArithmetic(stepT))
                throw typeError("FOR STEP value must be arithmetic", fs->loc);
        }
        analyseStmt(*fs->body);
        if (fs->untilCond) analyseExpr(*fs->untilCond);
    } else if (auto* ds = dynamic_cast<DoStmtNode*>(&node)) {
        if (ds->body) analyseStmt(*ds->body);
        if (ds->untilCond) analyseExpr(*ds->untilCond);
    } else if (auto* rs = dynamic_cast<RepeatStmtNode*>(&node)) {
        for (auto& s : rs->stmts) analyseStmt(*s);
        analyseExpr(*rs->cond);
    } else if (auto* cs = dynamic_cast<CaseStmtNode*>(&node)) {
        TypeSpec et = analyseExpr(*cs->expr);
        if (et.base != BaseType::INTEGER)
            throw typeError("CASE expression must be INTEGER", cs->loc);
        for (auto& arm : cs->arms) {
            auto* ca = dynamic_cast<CaseArmNode*>(arm.get());
            if (ca) analyseStmt(*ca->stmt);
        }
    } else if (auto* pc = dynamic_cast<ProcCallStmtNode*>(&node)) {
        Symbol* sym = lookup(pc->name, pc->loc);
        if (sym->kind != SymbolKind::PROCEDURE && sym->kind != SymbolKind::FUNCTION)
            throw SemanticError("'" + pc->name + "' is not a procedure", pc->loc);
        if (pc->args.size() != sym->params.size()) {
            throw SemanticError("Wrong number of arguments in call to '" + pc->name + "'", pc->loc);
        }
        for (size_t i = 0; i < pc->args.size(); ++i) {
            TypeSpec at = analyseExpr(*pc->args[i]);
            if (!compatible(at, sym->params[i].type))
                throw typeError("Argument type mismatch in call to '" + pc->name + "'", pc->loc);
        }
    } else if (auto* ret = dynamic_cast<ReturnStmtNode*>(&node)) {
        if (ret->value) {
            TypeSpec rt = analyseExpr(*ret->value);
            if (curReturnType_.base == BaseType::VOID)
                throw typeError("RETURN with value in void procedure", ret->loc);
            if (!compatible(rt, curReturnType_))
                throw typeError("RETURN type mismatch", ret->loc);
        } else {
            if (curReturnType_.base != BaseType::VOID)
                throw typeError("RETURN without value in function", ret->loc);
        }
    } else if (auto* ls = dynamic_cast<LabelStmtNode*>(&node)) {
        analyseStmt(*ls->stmt);
    } else if (auto* ps = dynamic_cast<PrintStmtNode*>(&node)) {
        // Parse format specifiers and validate against args
        std::vector<FmtSpec> specs;
        for (size_t i = 0; i < ps->fmt.size(); ++i) {
            if (ps->fmt[i] == '%' && i + 1 < ps->fmt.size()) {
                char c = ps->fmt[i + 1];
                switch (c) {
                case 'd': specs.push_back(FmtSpec::D); ++i; break;
                case 'l': specs.push_back(FmtSpec::L); ++i; break;
                case 'f': specs.push_back(FmtSpec::F); ++i; break;
                case 'c': specs.push_back(FmtSpec::C); ++i; break;
                case 's': specs.push_back(FmtSpec::S); ++i; break;
                case '%': ++i; break;  // escaped %%, not a specifier
                default:
                    throw SemanticError(
                        std::string("Unknown format specifier '%") + c + "' in PRINT",
                        ps->loc);
                }
            }
        }
        if (specs.size() != ps->args.size())
            throw SemanticError(
                "PRINT format has " + std::to_string(specs.size()) +
                " specifier(s) but " + std::to_string(ps->args.size()) +
                " argument(s) provided", ps->loc);

        for (size_t i = 0; i < specs.size(); ++i) {
            TypeSpec at = analyseExpr(*ps->args[i]);
            switch (specs[i]) {
            case FmtSpec::D:
            case FmtSpec::C:
                if (at.base != BaseType::INTEGER && at.base != BaseType::BIT)
                    throw typeError(
                        "%d/%c requires an integer argument (BYTE, WORD, or LONG)", ps->loc);
                break;
            case FmtSpec::L:
                if (at.base != BaseType::INTEGER && at.base != BaseType::BIT)
                    throw typeError("%l requires a LONG integer argument", ps->loc);
                break;
            case FmtSpec::F:
                if (at.base != BaseType::REAL && at.base != BaseType::INTEGER)
                    throw typeError("%f requires a REAL argument", ps->loc);
                break;
            case FmtSpec::S:
                if (at.base != BaseType::CHARACTER)
                    throw typeError("%s requires a CHARACTER argument", ps->loc);
                break;
            }
        }
        ps->specs = specs;
    }
    // GotoStmt, AsmStmt: no type checking needed
}

// ============================================================
// Expression type inference
// ============================================================

TypeSpec Sema::analyseExpr(ASTNode& node) {
    TypeSpec t;

    if (dynamic_cast<IntLiteralNode*>(&node)) {
        t.base = BaseType::INTEGER; t.width = 2; return t;
    }
    if (dynamic_cast<RealLiteralNode*>(&node)) {
        t.base = BaseType::REAL; t.width = 4; return t;
    }
    if (auto* n = dynamic_cast<StringLiteralNode*>(&node)) {
        t.base = BaseType::CHARACTER;
        t.width = static_cast<int>(n->value.size());
        t.strLen = t.width;
        return t;
    }
    if (dynamic_cast<BitLiteralNode*>(&node)) {
        t.base = BaseType::BIT; t.width = 2; return t;
    }
    if (dynamic_cast<AddressLiteralNode*>(&node)) {
        t.base = BaseType::INTEGER; t.width = 2; return t;
    }
    if (dynamic_cast<RegisterNode*>(&node)) {
        t.base = BaseType::INTEGER; t.width = 2; return t;
    }
    if (auto* n = dynamic_cast<IdentifierNode*>(&node)) {
        Symbol* sym = lookup(n->name, n->loc);
        return sym->type;
    }
    if (auto* n = dynamic_cast<BinaryExprNode*>(&node)) {
        TypeSpec lt = analyseExpr(*n->left);
        TypeSpec rt = analyseExpr(*n->right);
        if (!compatible(lt, rt))
            throw typeError("Incompatible operand types in binary expression", n->loc);
        // Relational: result is INTEGER (boolean)
        switch (n->op) {
        case TokenKind::TOK_EQ:  case TokenKind::TOK_NEQ:
        case TokenKind::TOK_LT:  case TokenKind::TOK_GT:
        case TokenKind::TOK_LEQ: case TokenKind::TOK_GEQ:
            t.base = BaseType::INTEGER; t.width = 2; return t;
        default:
            return promoteArith(lt, rt);
        }
    }
    if (auto* n = dynamic_cast<UnaryExprNode*>(&node)) {
        TypeSpec ot = analyseExpr(*n->operand);
        if (n->op == TokenKind::TOK_MINUS && !isArithmetic(ot))
            throw typeError("Unary minus requires arithmetic operand", n->loc);
        return ot;
    }
    if (auto* n = dynamic_cast<IndexExprNode*>(&node)) {
        TypeSpec bt = analyseExpr(*n->base);
        // Result type is the base element type
        return bt;
    }
    if (auto* n = dynamic_cast<DerefExprNode*>(&node)) {
        TypeSpec pt = analyseExpr(*n->ptr);
        // Dereferencing yields the pointed-to type (same type for now)
        return pt;
    }
    if (auto* n = dynamic_cast<AddrOfExprNode*>(&node)) {
        analyseExpr(*n->var);
        t.base = BaseType::INTEGER; t.width = 2; return t;
    }
    if (auto* n = dynamic_cast<FuncCallExprNode*>(&node)) {
        Symbol* sym = lookup(n->name, n->loc);
        // Array indexing: IDENTIFIER(i, j) where identifier is a variable
        if (sym->kind == SymbolKind::VARIABLE || sym->kind == SymbolKind::CONSTANT) {
            for (auto& idx : n->args) analyseExpr(*idx);
            return sym->type;  // element type (arrays share their base type)
        }
        if (sym->kind != SymbolKind::FUNCTION)
            throw SemanticError("'" + n->name + "' is not a function", n->loc);
        if (n->args.size() != sym->params.size())
            throw SemanticError("Wrong number of arguments in call to '" + n->name + "'", n->loc);
        for (size_t i = 0; i < n->args.size(); ++i)
            analyseExpr(*n->args[i]);
        return sym->retType;
    }

    // Default: return INTEGER
    t.base = BaseType::INTEGER; t.width = 2;
    return t;
}

} // namespace pl11
