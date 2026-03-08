#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "../lexer/pl11_lexer.h"

namespace pl11 {

// ============================================================
// Type representation
// ============================================================

enum class BaseType { INTEGER, REAL, CHARACTER, BIT, VOID };

struct TypeSpec {
    BaseType base    = BaseType::INTEGER;
    int      width   = 0;   // byte width; 0 = default (INTEGER→2, REAL→4, etc.)
    int      strLen  = 1;   // for CHARACTER
    bool     isArray = false;
    std::vector<int> dims;  // array dimensions

    // Convenience
    bool isInteger()   const { return base == BaseType::INTEGER; }
    bool isReal()      const { return base == BaseType::REAL; }
    bool isCharacter() const { return base == BaseType::CHARACTER; }
    bool isBit()       const { return base == BaseType::BIT; }
    bool isVoid()      const { return base == BaseType::VOID; }

    int effectiveWidth() const {
        if (width != 0) return width;
        switch (base) {
        case BaseType::INTEGER:   return 2;
        case BaseType::REAL:      return 4;
        case BaseType::CHARACTER: return strLen;
        case BaseType::BIT:       return 2;
        default:                  return 0;
        }
    }
};

// ============================================================
// Parameter passing mode
// ============================================================

enum class ParamMode { IN, OUT, IN_OUT };

struct ParamDecl {
    TypeSpec   type;
    std::string name;
    ParamMode  mode = ParamMode::IN;
};

// ============================================================
// Forward declarations
// ============================================================

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

// ============================================================
// AST Node kinds
// ============================================================

enum class NodeKind {
    // Statements
    PROGRAM,
    BLOCK,
    ASSIGN_STMT,
    IF_STMT,
    WHILE_STMT,
    FOR_STMT,
    REPEAT_STMT,
    CASE_STMT,
    CASE_ARM,
    PROC_CALL_STMT,
    RETURN_STMT,
    GOTO_STMT,
    LABEL_STMT,
    ASM_STMT,
    PRINT_STMT,

    // Expressions
    INT_LITERAL,
    REAL_LITERAL,
    STRING_LITERAL,
    BIT_LITERAL,
    ADDRESS_LITERAL,
    IDENTIFIER_EXPR,
    REGISTER_EXPR,
    BINARY_EXPR,
    UNARY_EXPR,
    INDEX_EXPR,         // array[i, j]
    DEREF_EXPR,         // ptr^
    ADDR_OF_EXPR,       // @var
    FUNC_CALL_EXPR,

    // Declarations
    VAR_DECL,
    CONST_DECL,
    PROC_DECL,
};

// ============================================================
// Base AST node
// ============================================================

struct ASTNode {
    NodeKind  kind;
    SourceLoc loc;

    explicit ASTNode(NodeKind k, SourceLoc l = {}) : kind(k), loc(l) {}
    virtual ~ASTNode() = default;
};

// ============================================================
// Literal nodes
// ============================================================

struct IntLiteralNode : ASTNode {
    long long value;
    IntLiteralNode(long long v, SourceLoc l)
        : ASTNode(NodeKind::INT_LITERAL, l), value(v) {}
};

struct RealLiteralNode : ASTNode {
    double value;
    RealLiteralNode(double v, SourceLoc l)
        : ASTNode(NodeKind::REAL_LITERAL, l), value(v) {}
};

struct StringLiteralNode : ASTNode {
    std::string value;
    StringLiteralNode(std::string v, SourceLoc l)
        : ASTNode(NodeKind::STRING_LITERAL, l), value(std::move(v)) {}
};

struct BitLiteralNode : ASTNode {
    long long intValue;   // bit pattern as integer
    std::string bitStr;   // original '0101...' string
    BitLiteralNode(long long iv, std::string bs, SourceLoc l)
        : ASTNode(NodeKind::BIT_LITERAL, l), intValue(iv), bitStr(std::move(bs)) {}
};

struct AddressLiteralNode : ASTNode {
    long long address;
    AddressLiteralNode(long long addr, SourceLoc l)
        : ASTNode(NodeKind::ADDRESS_LITERAL, l), address(addr) {}
};

// ============================================================
// Variable / expression nodes
// ============================================================

struct IdentifierNode : ASTNode {
    std::string name;
    IdentifierNode(std::string n, SourceLoc l)
        : ASTNode(NodeKind::IDENTIFIER_EXPR, l), name(std::move(n)) {}
};

struct RegisterNode : ASTNode {
    int regNum;   // 0-15
    RegisterNode(int r, SourceLoc l)
        : ASTNode(NodeKind::REGISTER_EXPR, l), regNum(r) {}
};

struct BinaryExprNode : ASTNode {
    TokenKind       op;
    ASTNodePtr      left;
    ASTNodePtr      right;
    BinaryExprNode(TokenKind op, ASTNodePtr l, ASTNodePtr r, SourceLoc loc)
        : ASTNode(NodeKind::BINARY_EXPR, loc), op(op),
          left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExprNode : ASTNode {
    TokenKind  op;
    ASTNodePtr operand;
    UnaryExprNode(TokenKind op, ASTNodePtr operand, SourceLoc loc)
        : ASTNode(NodeKind::UNARY_EXPR, loc), op(op), operand(std::move(operand)) {}
};

struct IndexExprNode : ASTNode {
    ASTNodePtr              base;
    std::vector<ASTNodePtr> indices;
    IndexExprNode(ASTNodePtr b, std::vector<ASTNodePtr> idx, SourceLoc l)
        : ASTNode(NodeKind::INDEX_EXPR, l), base(std::move(b)), indices(std::move(idx)) {}
};

struct DerefExprNode : ASTNode {
    ASTNodePtr ptr;
    explicit DerefExprNode(ASTNodePtr p, SourceLoc l)
        : ASTNode(NodeKind::DEREF_EXPR, l), ptr(std::move(p)) {}
};

struct AddrOfExprNode : ASTNode {
    ASTNodePtr var;
    explicit AddrOfExprNode(ASTNodePtr v, SourceLoc l)
        : ASTNode(NodeKind::ADDR_OF_EXPR, l), var(std::move(v)) {}
};

struct FuncCallExprNode : ASTNode {
    std::string             name;
    std::vector<ASTNodePtr> args;
    FuncCallExprNode(std::string n, std::vector<ASTNodePtr> a, SourceLoc l)
        : ASTNode(NodeKind::FUNC_CALL_EXPR, l), name(std::move(n)), args(std::move(a)) {}
};

// ============================================================
// Statement nodes
// ============================================================

struct AssignStmtNode : ASTNode {
    ASTNodePtr target;
    ASTNodePtr value;
    AssignStmtNode(ASTNodePtr t, ASTNodePtr v, SourceLoc l)
        : ASTNode(NodeKind::ASSIGN_STMT, l), target(std::move(t)), value(std::move(v)) {}
};

struct IfStmtNode : ASTNode {
    ASTNodePtr cond;
    ASTNodePtr thenStmt;
    ASTNodePtr elseStmt;  // may be null
    IfStmtNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e, SourceLoc l)
        : ASTNode(NodeKind::IF_STMT, l), cond(std::move(c)),
          thenStmt(std::move(t)), elseStmt(std::move(e)) {}
};

struct WhileStmtNode : ASTNode {
    ASTNodePtr cond;
    ASTNodePtr body;
    ASTNodePtr untilCond;  // null if no UNTIL clause
    WhileStmtNode(ASTNodePtr c, ASTNodePtr b, ASTNodePtr u, SourceLoc l)
        : ASTNode(NodeKind::WHILE_STMT, l), cond(std::move(c)),
          body(std::move(b)), untilCond(std::move(u)) {}
};

struct ForStmtNode : ASTNode {
    std::string loopVar;
    ASTNodePtr  start;
    ASTNodePtr  end;
    bool        downto;   // true → DOWNTO, false → TO
    ASTNodePtr  body;
    ASTNodePtr  untilCond;  // null if no UNTIL clause
    ForStmtNode(std::string v, ASTNodePtr s, ASTNodePtr e, bool dt,
                ASTNodePtr b, ASTNodePtr u, SourceLoc l)
        : ASTNode(NodeKind::FOR_STMT, l), loopVar(std::move(v)),
          start(std::move(s)), end(std::move(e)), downto(dt),
          body(std::move(b)), untilCond(std::move(u)) {}
};

struct RepeatStmtNode : ASTNode {
    std::vector<ASTNodePtr> stmts;
    ASTNodePtr              cond;
    RepeatStmtNode(std::vector<ASTNodePtr> s, ASTNodePtr c, SourceLoc l)
        : ASTNode(NodeKind::REPEAT_STMT, l), stmts(std::move(s)), cond(std::move(c)) {}
};

struct CaseArmNode : ASTNode {
    long long  value;
    ASTNodePtr stmt;
    CaseArmNode(long long v, ASTNodePtr s, SourceLoc l)
        : ASTNode(NodeKind::CASE_ARM, l), value(v), stmt(std::move(s)) {}
};

struct CaseStmtNode : ASTNode {
    ASTNodePtr              expr;
    std::vector<ASTNodePtr> arms;  // CaseArmNode
    CaseStmtNode(ASTNodePtr e, std::vector<ASTNodePtr> a, SourceLoc l)
        : ASTNode(NodeKind::CASE_STMT, l), expr(std::move(e)), arms(std::move(a)) {}
};

struct ProcCallStmtNode : ASTNode {
    std::string             name;
    std::vector<ASTNodePtr> args;
    ProcCallStmtNode(std::string n, std::vector<ASTNodePtr> a, SourceLoc l)
        : ASTNode(NodeKind::PROC_CALL_STMT, l), name(std::move(n)), args(std::move(a)) {}
};

struct ReturnStmtNode : ASTNode {
    ASTNodePtr value;  // may be null for void procedures
    explicit ReturnStmtNode(ASTNodePtr v, SourceLoc l)
        : ASTNode(NodeKind::RETURN_STMT, l), value(std::move(v)) {}
};

struct GotoStmtNode : ASTNode {
    std::string label;
    GotoStmtNode(std::string lbl, SourceLoc l)
        : ASTNode(NodeKind::GOTO_STMT, l), label(std::move(lbl)) {}
};

struct LabelStmtNode : ASTNode {
    std::string label;
    ASTNodePtr  stmt;
    LabelStmtNode(std::string lbl, ASTNodePtr s, SourceLoc l)
        : ASTNode(NodeKind::LABEL_STMT, l), label(std::move(lbl)), stmt(std::move(s)) {}
};

struct AsmStmtNode : ASTNode {
    std::string asmText;
    AsmStmtNode(std::string text, SourceLoc l)
        : ASTNode(NodeKind::ASM_STMT, l), asmText(std::move(text)) {}
};

// Format specifier kinds recognised in a PRINT format string.
enum class FmtSpec { D, L, F, C, S };

struct PrintStmtNode : ASTNode {
    std::string             fmt;   // raw format string from source
    std::vector<ASTNodePtr> args;  // one per format specifier
    std::vector<FmtSpec>    specs; // parallel to args; filled by sema
    PrintStmtNode(std::string f, std::vector<ASTNodePtr> a, SourceLoc l)
        : ASTNode(NodeKind::PRINT_STMT, l), fmt(std::move(f)), args(std::move(a)) {}
};

// ============================================================
// Block node
// ============================================================

struct BlockNode : ASTNode {
    std::vector<ASTNodePtr> decls;
    std::vector<ASTNodePtr> stmts;
    BlockNode(SourceLoc l) : ASTNode(NodeKind::BLOCK, l) {}
};

// ============================================================
// Declaration nodes
// ============================================================

struct VarDeclNode : ASTNode {
    TypeSpec    type;
    std::string name;
    std::vector<int> dims;  // array dimensions (empty if scalar)
    VarDeclNode(TypeSpec t, std::string n, std::vector<int> d, SourceLoc l)
        : ASTNode(NodeKind::VAR_DECL, l), type(t), name(std::move(n)), dims(std::move(d)) {}
};

struct ConstDeclNode : ASTNode {
    TypeSpec    type;
    std::string name;
    ASTNodePtr  value;
    ConstDeclNode(TypeSpec t, std::string n, ASTNodePtr v, SourceLoc l)
        : ASTNode(NodeKind::CONST_DECL, l), type(t), name(std::move(n)), value(std::move(v)) {}
};

struct ProcDeclNode : ASTNode {
    TypeSpec                retType;   // VOID if not a function
    std::string             name;
    std::vector<ParamDecl>  params;
    std::unique_ptr<BlockNode> body;   // null for FORWARD declarations
    bool                    isForward = false;
    ProcDeclNode(TypeSpec rt, std::string n, std::vector<ParamDecl> p,
                 std::unique_ptr<BlockNode> b, SourceLoc l)
        : ASTNode(NodeKind::PROC_DECL, l), retType(rt), name(std::move(n)),
          params(std::move(p)), body(std::move(b)) {}
};

// ============================================================
// Program root
// ============================================================

struct ProgramNode : ASTNode {
    std::unique_ptr<BlockNode> block;
    ProgramNode(std::unique_ptr<BlockNode> b, SourceLoc l)
        : ASTNode(NodeKind::PROGRAM, l), block(std::move(b)) {}
};

} // namespace pl11
