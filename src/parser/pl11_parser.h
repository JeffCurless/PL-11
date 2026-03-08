#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "../lexer/pl11_lexer.h"
#include "../ast/pl11_ast.h"

namespace pl11 {

// ============================================================
// Parse error
// ============================================================

struct ParseError : std::runtime_error {
    SourceLoc loc;
    ParseError(const std::string& msg, SourceLoc l)
        : std::runtime_error(msg), loc(l) {}
};

// ============================================================
// Parser (recursive-descent LL(1))
// ============================================================

class Parser {
public:
    explicit Parser(Lexer& lexer);

    std::unique_ptr<ProgramNode> parseProgram();

private:
    Lexer& lex_;
    Token  cur_;   // current (lookahead) token

    // Token consumption helpers
    Token advance();
    Token expect(TokenKind k);
    bool  check(TokenKind k) const;
    bool  match(TokenKind k);

    // Grammar rules — one function per rule
    std::unique_ptr<BlockNode>       parseBlock();
    std::vector<ASTNodePtr>          parseDeclSection();
    std::vector<ASTNodePtr>          parseStmtList();
    // parseDeclarationGroup appends one or more decls (handles comma-separated var lists)
    void                             parseDeclarationGroup(std::vector<ASTNodePtr>& out);
    ASTNodePtr                       parseDeclaration();  // returns first from group
    void                             parseVarDeclGroup(TypeSpec ts, std::vector<ASTNodePtr>& out);
    ASTNodePtr                       parseConstDecl();
    ASTNodePtr                       parseProcDecl(TypeSpec retType = TypeSpec{});
    TypeSpec                         parseTypeSpec();
    std::vector<ParamDecl>           parseParamList();
    ParamDecl                        parseParamDecl();
    ParamMode                        parseParamMode();

    ASTNodePtr parseStatement();
    ASTNodePtr parseExprStmt();          // expression => lvalue  OR  lvalue := expression
    ASTNodePtr parseLValue();            // parse assignment target for => form
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseDoStmt();
    ASTNodePtr parseRepeatStmt();
    ASTNodePtr parseCaseStmt();
    ASTNodePtr parseProcCallStmt(std::string name, SourceLoc loc);
    ASTNodePtr parseReturnStmt();
    ASTNodePtr parseGotoStmt();
    ASTNodePtr parseAsmStmt();
    ASTNodePtr parsePrintStmt();
    ASTNodePtr parsePushStmt();
    ASTNodePtr parsePopStmt();

    // Expressions
    ASTNodePtr parseExpression();
    ASTNodePtr parseRelExpr();
    ASTNodePtr parseAddExpr();
    ASTNodePtr parseMulExpr();
    ASTNodePtr parseUnaryExpr();
    ASTNodePtr parsePostfixExpr();
    ASTNodePtr parsePrimaryExpr();
    std::vector<ASTNodePtr> parseArgList();

    // Utilities
    ParseError error(const std::string& msg) const;
    bool isRelOp(TokenKind k) const;
    bool isAddOp(TokenKind k) const;
    bool isMulOp(TokenKind k) const;
    bool isTypeKeyword(TokenKind k) const;
    bool isStatementStart() const;
    std::string expectIdentifier();
};

} // namespace pl11
