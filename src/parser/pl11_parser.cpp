#include "pl11_parser.h"
#include <sstream>

namespace pl11 {

Parser::Parser(Lexer& lexer) : lex_(lexer) {
    cur_ = lex_.next();
}

// ============================================================
// Token helpers
// ============================================================

Token Parser::advance() {
    Token prev = cur_;
    cur_ = lex_.next();
    return prev;
}

Token Parser::expect(TokenKind k) {
    if (cur_.kind != k) {
        std::ostringstream oss;
        oss << "Expected " << tokenKindName(k) << " but got '"
            << cur_.text << "' (" << tokenKindName(cur_.kind) << ")";
        throw error(oss.str());
    }
    return advance();
}

bool Parser::check(TokenKind k) const { return cur_.kind == k; }

bool Parser::match(TokenKind k) {
    if (check(k)) { advance(); return true; }
    return false;
}

ParseError Parser::error(const std::string& msg) const {
    std::string full = cur_.loc.file + ":" + std::to_string(cur_.loc.line) +
                       ":" + std::to_string(cur_.loc.col) + ": error: " + msg;
    return ParseError(full, cur_.loc);
}

std::string Parser::expectIdentifier() {
    if (cur_.kind != TokenKind::TOK_IDENTIFIER) {
        throw error("Expected identifier, got '" + cur_.text + "'");
    }
    std::string name = cur_.strVal.empty() ? cur_.text : cur_.strVal;
    advance();
    return name;
}

// ============================================================
// Type predicates
// ============================================================

bool Parser::isRelOp(TokenKind k) const {
    return k == TokenKind::TOK_EQ  || k == TokenKind::TOK_NEQ ||
           k == TokenKind::TOK_LT  || k == TokenKind::TOK_GT  ||
           k == TokenKind::TOK_LEQ || k == TokenKind::TOK_GEQ;
}

bool Parser::isAddOp(TokenKind k) const {
    return k == TokenKind::TOK_PLUS  || k == TokenKind::TOK_MINUS ||
           k == TokenKind::TOK_OR    || k == TokenKind::TOK_XOR;
}

bool Parser::isMulOp(TokenKind k) const {
    return k == TokenKind::TOK_STAR  || k == TokenKind::TOK_SLASH ||
           k == TokenKind::TOK_MOD   || k == TokenKind::TOK_AND   ||
           k == TokenKind::TOK_SHL   || k == TokenKind::TOK_SHR   ||
           k == TokenKind::TOK_SHRA;
}

bool Parser::isTypeKeyword(TokenKind k) const {
    return k == TokenKind::TOK_BYTE      || k == TokenKind::TOK_WORD      ||
           k == TokenKind::TOK_LONG      || k == TokenKind::TOK_INTEGER   ||
           k == TokenKind::TOK_REAL      || k == TokenKind::TOK_FLOAT     ||
           k == TokenKind::TOK_CHARACTER || k == TokenKind::TOK_BIT;
}

bool Parser::isStatementStart() const {
    switch (cur_.kind) {
    // Keyword-headed statements
    case TokenKind::TOK_BEGIN:
    case TokenKind::TOK_IF:
    case TokenKind::TOK_WHILE:
    case TokenKind::TOK_FOR:
    case TokenKind::TOK_DO:
    case TokenKind::TOK_REPEAT:
    case TokenKind::TOK_CASE:
    case TokenKind::TOK_CALL:
    case TokenKind::TOK_RETURN:
    case TokenKind::TOK_GOTO:
    case TokenKind::TOK_ASM:
    case TokenKind::TOK_PRINT:
    case TokenKind::TOK_PUSH:
    case TokenKind::TOK_POP:
    // Expression-headed statements (assignment / label)
    case TokenKind::TOK_IDENTIFIER:
    case TokenKind::TOK_REGISTER:
    case TokenKind::TOK_INTEGER_LIT:
    case TokenKind::TOK_REAL_LIT:
    case TokenKind::TOK_STRING_LIT:
    case TokenKind::TOK_BIT_LIT:
    case TokenKind::TOK_ADDRESS_LIT:
    case TokenKind::TOK_MINUS:
    case TokenKind::TOK_NOT:
    case TokenKind::TOK_LPAREN:
    case TokenKind::TOK_AT:
        return true;
    default:
        return false;
    }
}

// ============================================================
// Top-level
// ============================================================

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    SourceLoc loc = cur_.loc;
    auto block = parseBlock();
    expect(TokenKind::TOK_EOF);
    return std::make_unique<ProgramNode>(std::move(block), loc);
}

// ============================================================
// Block
// ============================================================

std::unique_ptr<BlockNode> Parser::parseBlock() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_BEGIN);

    auto block = std::make_unique<BlockNode>(loc);

    // Declaration section: declarations start with a type keyword, PROCEDURE, or CONSTANT.
    // (ARRAY no longer starts a declaration — it now appears mid-declaration after the size.)
    while (true) {
        if (isTypeKeyword(cur_.kind) || cur_.kind == TokenKind::TOK_PROCEDURE ||
            cur_.kind == TokenKind::TOK_CONSTANT) {
            parseDeclarationGroup(block->decls);
            if (check(TokenKind::TOK_SEMI))
                advance();
        } else {
            break;
        }
    }

    // Statement list
    block->stmts = parseStmtList();

    expect(TokenKind::TOK_END);
    return block;
}

std::vector<ASTNodePtr> Parser::parseDeclSection() {
    std::vector<ASTNodePtr> decls;
    while (isTypeKeyword(cur_.kind) || cur_.kind == TokenKind::TOK_PROCEDURE ||
           cur_.kind == TokenKind::TOK_CONSTANT) {
        parseDeclarationGroup(decls);
        if (check(TokenKind::TOK_SEMI)) advance();
    }
    return decls;
}

void Parser::parseDeclarationGroup(std::vector<ASTNodePtr>& out) {
    if (cur_.kind == TokenKind::TOK_CONSTANT) {
        out.push_back(parseConstDecl());
        return;
    }
    if (cur_.kind == TokenKind::TOK_PROCEDURE) {
        out.push_back(parseProcDecl());
        return;
    }
    // Type keyword: var decl, array decl, or typed procedure
    TypeSpec ts = parseTypeSpec();
    if (cur_.kind == TokenKind::TOK_PROCEDURE) {
        out.push_back(parseProcDecl(ts));  // pass return type
        return;
    }
    // Variable / array declarations
    parseVarDeclGroup(ts, out);
}

std::vector<ASTNodePtr> Parser::parseStmtList() {
    std::vector<ASTNodePtr> stmts;
    while (isStatementStart()) {
        stmts.push_back(parseStatement());
        if (check(TokenKind::TOK_SEMI)) advance();
    }
    return stmts;
}

// ============================================================
// Declarations
// ============================================================

ASTNodePtr Parser::parseDeclaration() {
    std::vector<ASTNodePtr> out;
    parseDeclarationGroup(out);
    if (out.empty()) throw error("Empty declaration");
    // Return first (for callers that only expect one; parseDeclarationGroup handles lists)
    return std::move(out.front());
}

void Parser::parseVarDeclGroup(TypeSpec ts, std::vector<ASTNodePtr>& out) {
    // UNH array form: TYPE size ARRAY name[, name ...]
    // Detected when the token after the type spec is an integer literal.
    // e.g.:  WORD 5 ARRAY VALUES;    WORD 3, 4 ARRAY MATRIX;
    if (check(TokenKind::TOK_INTEGER_LIT)) {
        SourceLoc loc = cur_.loc;
        std::vector<int> dims;
        dims.push_back(static_cast<int>(cur_.intVal));
        advance();
        while (match(TokenKind::TOK_COMMA)) {
            if (!check(TokenKind::TOK_INTEGER_LIT))
                throw error("Expected integer dimension in array declaration");
            dims.push_back(static_cast<int>(cur_.intVal));
            advance();
        }
        expect(TokenKind::TOK_ARRAY);
        do {
            std::string name = expectIdentifier();
            out.push_back(std::make_unique<VarDeclNode>(ts, name, dims, loc));
        } while (match(TokenKind::TOK_COMMA));
        return;
    }

    // Traditional form: name[, name] or name(dim[, dim...])[, ...]
    do {
        SourceLoc loc = cur_.loc;
        std::string name = expectIdentifier();
        std::vector<int> dims;
        if (match(TokenKind::TOK_LPAREN)) {
            do {
                auto dimExpr = parseExpression();
                if (auto* lit = dynamic_cast<IntLiteralNode*>(dimExpr.get()))
                    dims.push_back(static_cast<int>(lit->value));
                else
                    throw error("Array dimension must be an integer constant");
            } while (match(TokenKind::TOK_COMMA));
            expect(TokenKind::TOK_RPAREN);
        }
        out.push_back(std::make_unique<VarDeclNode>(ts, name, dims, loc));
    } while (match(TokenKind::TOK_COMMA));
}

ASTNodePtr Parser::parseConstDecl() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_CONSTANT);
    TypeSpec ts = parseTypeSpec();
    std::string name = expectIdentifier();
    expect(TokenKind::TOK_EQ);
    ASTNodePtr val = parsePrimaryExpr();  // only literals allowed
    return std::make_unique<ConstDeclNode>(ts, name, std::move(val), loc);
}

ASTNodePtr Parser::parseProcDecl(TypeSpec retType) {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_PROCEDURE);

    // FORWARD declaration?
    if (cur_.kind == TokenKind::TOK_FORWARD) {
        advance();
        std::string name = expectIdentifier();
        auto node = std::make_unique<ProcDeclNode>(retType, name,
                                                   std::vector<ParamDecl>{}, nullptr, loc);
        node->isForward = true;
        return node;
    }

    std::string name = expectIdentifier();

    std::vector<ParamDecl> params;
    if (match(TokenKind::TOK_LPAREN)) {
        params = parseParamList();
        expect(TokenKind::TOK_RPAREN);
    }

    expect(TokenKind::TOK_SEMI);
    auto body = parseBlock();
    // Optional trailing semicolon after END in procedure body
    // (handled at call site)
    return std::make_unique<ProcDeclNode>(retType, name, std::move(params), std::move(body), loc);
}

TypeSpec Parser::parseTypeSpec() {
    TypeSpec ts;
    switch (cur_.kind) {
    case TokenKind::TOK_BYTE:
        ts.base = BaseType::INTEGER; ts.width = 1; advance(); return ts;
    case TokenKind::TOK_WORD:
    case TokenKind::TOK_INTEGER:   // INTEGER kept as alias for WORD
        ts.base = BaseType::INTEGER; ts.width = 2; advance(); break;
    case TokenKind::TOK_LONG:
        ts.base = BaseType::INTEGER; ts.width = 4; advance(); return ts;
    case TokenKind::TOK_REAL:
    case TokenKind::TOK_FLOAT:
        ts.base = BaseType::REAL; ts.width = 4; advance(); break;
    case TokenKind::TOK_CHARACTER:
        ts.base = BaseType::CHARACTER; ts.width = 1; advance(); break;
    case TokenKind::TOK_BIT:
        ts.base = BaseType::BIT; ts.width = 2; advance(); break;
    default:
        throw error("Expected type keyword (BYTE, WORD, LONG, REAL, CHARACTER, BIT)");
    }

    // Optional *width (still supported for REAL*8, CHARACTER*20, BIT*16, etc.)
    if (match(TokenKind::TOK_STAR)) {
        if (cur_.kind != TokenKind::TOK_INTEGER_LIT)
            throw error("Expected integer width after '*' in type specification");
        ts.width = static_cast<int>(cur_.intVal);
        if (ts.base == BaseType::CHARACTER) ts.strLen = ts.width;
        advance();
    }
    return ts;
}

std::vector<ParamDecl> Parser::parseParamList() {
    std::vector<ParamDecl> params;
    params.push_back(parseParamDecl());
    // Accept both ';' and ',' as param separators
    while (check(TokenKind::TOK_SEMI) || check(TokenKind::TOK_COMMA)) {
        advance();
        params.push_back(parseParamDecl());
    }
    return params;
}

ParamDecl Parser::parseParamDecl() {
    ParamDecl pd;
    pd.mode = parseParamMode();
    pd.type = parseTypeSpec();
    pd.name = expectIdentifier();
    // Handle comma-separated names: only parse first for simplicity
    return pd;
}

ParamMode Parser::parseParamMode() {
    if (cur_.kind == TokenKind::TOK_IN) {
        advance();
        if (cur_.kind == TokenKind::TOK_OUT) { advance(); return ParamMode::IN_OUT; }
        return ParamMode::IN;
    }
    if (cur_.kind == TokenKind::TOK_OUT) { advance(); return ParamMode::OUT; }
    return ParamMode::IN;  // default
}

// ============================================================
// Statements
// ============================================================

ASTNodePtr Parser::parseStatement() {
    SourceLoc loc = cur_.loc;

    switch (cur_.kind) {
    case TokenKind::TOK_BEGIN:
        return parseBlock();
    case TokenKind::TOK_IF:
        return parseIfStmt();
    case TokenKind::TOK_WHILE:
        return parseWhileStmt();
    case TokenKind::TOK_FOR:
        return parseForStmt();
    case TokenKind::TOK_DO:
        return parseDoStmt();
    case TokenKind::TOK_REPEAT:
        return parseRepeatStmt();
    case TokenKind::TOK_CASE:
        return parseCaseStmt();
    case TokenKind::TOK_CALL:
        advance();
        return parseProcCallStmt(expectIdentifier(), loc);
    case TokenKind::TOK_RETURN:
        return parseReturnStmt();
    case TokenKind::TOK_GOTO:
        return parseGotoStmt();
    case TokenKind::TOK_ASM:
        return parseAsmStmt();
    case TokenKind::TOK_PRINT:
        return parsePrintStmt();
    case TokenKind::TOK_PUSH:
        return parsePushStmt();
    case TokenKind::TOK_POP:
        return parsePopStmt();
    // Expression-headed statements: assignment (both := and => forms) and labels
    case TokenKind::TOK_IDENTIFIER:
    case TokenKind::TOK_REGISTER:
    case TokenKind::TOK_INTEGER_LIT:
    case TokenKind::TOK_REAL_LIT:
    case TokenKind::TOK_STRING_LIT:
    case TokenKind::TOK_BIT_LIT:
    case TokenKind::TOK_ADDRESS_LIT:
    case TokenKind::TOK_MINUS:
    case TokenKind::TOK_NOT:
    case TokenKind::TOK_LPAREN:
    case TokenKind::TOK_AT:
        return parseExprStmt();
    default:
        throw error("Expected statement, got '" + cur_.text + "'");
    }
}

// Parse an expression-headed statement:
//   value => lvalue        (UNH assignment: value on left, target on right)
//   identifier : statement (label)
ASTNodePtr Parser::parseExprStmt() {
    SourceLoc loc = cur_.loc;

    // Special case: identifier followed by ':' alone (not ':=') is a label.
    // We detect this before parsing as expression to avoid consuming the identifier.
    if (check(TokenKind::TOK_IDENTIFIER)) {
        // Peek: if the token AFTER the identifier is ':', it's a label.
        // We can check by saving the identifier and peeking.
        // Use strVal if available (normalized uppercase), else fall back to text.
        std::string name = cur_.strVal.empty() ? cur_.text : cur_.strVal;
        SourceLoc nameLoc = cur_.loc;
        advance();  // consume identifier
        if (check(TokenKind::TOK_COLON)) {
            advance();  // consume ':'
            auto stmt = parseStatement();
            return std::make_unique<LabelStmtNode>(name, std::move(stmt), nameLoc);
        }
        // Not a label — reconstruct and fall through to expression parsing.
        // We already consumed the identifier, so build an IdentifierNode as the
        // start of the expression, then continue parsing postfix/binary ops.
        // Re-inject: temporarily parse the rest of the expression manually.
        // Build identifier node, then continue with postfix (subscript, deref, binary ops).
        // First postfix: function call or array subscript.
        // Use FuncCallExprNode for identifier(args) so sema/codegen can
        // distinguish function calls from array subscripts, matching parsePrimaryExpr.
        ASTNodePtr expr;
        if (check(TokenKind::TOK_LPAREN)) {
            advance();
            std::vector<ASTNodePtr> args;
            if (!check(TokenKind::TOK_RPAREN))
                args = parseArgList();
            expect(TokenKind::TOK_RPAREN);
            expr = std::make_unique<FuncCallExprNode>(name, std::move(args), nameLoc);
        } else {
            expr = std::make_unique<IdentifierNode>(name, nameLoc);
        }
        // Additional postfix: deref or further subscript
        while (check(TokenKind::TOK_LPAREN) || check(TokenKind::TOK_DEREF)) {
            if (check(TokenKind::TOK_LPAREN)) {
                advance();
                auto args = parseArgList();
                expect(TokenKind::TOK_RPAREN);
                expr = std::make_unique<IndexExprNode>(std::move(expr), std::move(args), nameLoc);
            } else {
                advance();
                expr = std::make_unique<DerefExprNode>(std::move(expr), nameLoc);
            }
        }
        // Now check for assignment operator or continue as binary expression
        if (check(TokenKind::TOK_ARROW)) {
            advance();
            auto target = parseLValue();
            return std::make_unique<AssignStmtNode>(std::move(target), std::move(expr), loc);
        }
        // Not assignment yet — binary expression (e.g. A MOD B => R, A + B => SUM).
        // Rebuild the expression hierarchy: mul → add → rel → expect =>.
        while (isMulOp(cur_.kind)) {
            TokenKind op = cur_.kind;
            advance();
            auto rhs = parseUnaryExpr();
            expr = std::make_unique<BinaryExprNode>(op, std::move(expr), std::move(rhs), loc);
        }
        while (isAddOp(cur_.kind)) {
            TokenKind op = cur_.kind;
            advance();
            auto rhs = parseMulExpr();
            expr = std::make_unique<BinaryExprNode>(op, std::move(expr), std::move(rhs), loc);
        }
        if (isRelOp(cur_.kind)) {
            TokenKind op = cur_.kind;
            advance();
            auto rhs = parseAddExpr();
            expr = std::make_unique<BinaryExprNode>(op, std::move(expr), std::move(rhs), loc);
            while (isRelOp(cur_.kind)) {
                op = cur_.kind;
                advance();
                rhs = parseAddExpr();
                expr = std::make_unique<BinaryExprNode>(op, std::move(expr), std::move(rhs), loc);
            }
        }
        // Now must be => (or error)
        if (check(TokenKind::TOK_ARROW)) {
            advance();
            auto target = parseLValue();
            return std::make_unique<AssignStmtNode>(std::move(target), std::move(expr), loc);
        }
        throw error("Expected '=>' in assignment statement");
    }

    // Non-identifier expression-headed statement (literal, unary, paren, register, @):
    // Parse full expression, then expect =>.
    auto value = parseExpression();
    expect(TokenKind::TOK_ARROW);
    auto target = parseLValue();
    return std::make_unique<AssignStmtNode>(std::move(target), std::move(value), loc);
}

// Parse an assignment target (lvalue) for the => form.
// Grammar: identifier [ '(' args ')' ] { '^' }   |   register { '^' }
ASTNodePtr Parser::parseLValue() {
    SourceLoc loc = cur_.loc;
    ASTNodePtr lval;

    if (check(TokenKind::TOK_IDENTIFIER)) {
        std::string name = cur_.strVal.empty() ? cur_.text : cur_.strVal;
        advance();
        lval = std::make_unique<IdentifierNode>(name, loc);
        if (check(TokenKind::TOK_LPAREN)) {
            advance();
            auto indices = parseArgList();
            expect(TokenKind::TOK_RPAREN);
            lval = std::make_unique<IndexExprNode>(std::move(lval), std::move(indices), loc);
        }
    } else if (check(TokenKind::TOK_REGISTER)) {
        lval = std::make_unique<RegisterNode>(cur_.regNum, loc);
        advance();
    } else if (check(TokenKind::TOK_SP)) {
        lval = std::make_unique<RegisterNode>(6, loc);
        advance();
    } else if (check(TokenKind::TOK_PC)) {
        lval = std::make_unique<RegisterNode>(7, loc);
        advance();
    } else {
        throw error("Expected assignment target (variable or register), got '" + cur_.text + "'");
    }

    // Dereference chain
    while (check(TokenKind::TOK_DEREF)) {
        advance();
        lval = std::make_unique<DerefExprNode>(std::move(lval), loc);
    }
    return lval;
}

ASTNodePtr Parser::parseIfStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_IF);
    auto cond = parseExpression();
    expect(TokenKind::TOK_THEN);
    auto thenStmt = parseStatement();
    ASTNodePtr elseStmt;
    if (match(TokenKind::TOK_ELSE))
        elseStmt = parseStatement();
    return std::make_unique<IfStmtNode>(std::move(cond), std::move(thenStmt),
                                        std::move(elseStmt), loc);
}

ASTNodePtr Parser::parseWhileStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_WHILE);
    auto cond = parseExpression();
    expect(TokenKind::TOK_DO);
    auto body = parseStatement();
    ASTNodePtr untilCond;
    if (match(TokenKind::TOK_UNTIL))
        untilCond = parseExpression();
    return std::make_unique<WhileStmtNode>(std::move(cond), std::move(body),
                                           std::move(untilCond), loc);
}

ASTNodePtr Parser::parseForStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_FOR);
    std::string var = expectIdentifier();

    // Two syntaxes:
    //   FOR var FROM start [STEP expr] [TO|DOWNTO] end DO ... (UNH with start)
    //   FOR var [STEP expr] [TO|DOWNTO] end DO ...            (UNH: use current value)
    ASTNodePtr start, step;
    if (match(TokenKind::TOK_FROM)) {
        start = parseExpression();
        if (match(TokenKind::TOK_STEP))
            step = parseExpression();
    } else {
        // No initialiser — loop starts at the variable's current value.
        // Next token must be STEP, TO, or DOWNTO.
        if (match(TokenKind::TOK_STEP))
            step = parseExpression();
        // start remains null
    }

    bool downto = false;
    if (match(TokenKind::TOK_DOWNTO)) downto = true;
    else expect(TokenKind::TOK_TO);

    auto end = parseExpression();
    expect(TokenKind::TOK_DO);
    auto body = parseStatement();
    ASTNodePtr untilCond;
    if (match(TokenKind::TOK_UNTIL))
        untilCond = parseExpression();
    return std::make_unique<ForStmtNode>(var, std::move(start), std::move(end),
                                         std::move(step), downto,
                                         std::move(body), std::move(untilCond), loc);
}

ASTNodePtr Parser::parseDoStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_DO);
    ASTNodePtr body;
    if (check(TokenKind::TOK_SEMI)) {
        advance();  // consume null-body ';' (e.g. DO ; or DO ; UNTIL cond)
    } else if (isStatementStart()) {
        body = parseStatement();
    }
    ASTNodePtr untilCond;
    if (match(TokenKind::TOK_UNTIL))
        untilCond = parseExpression();
    return std::make_unique<DoStmtNode>(std::move(body), std::move(untilCond), loc);
}

ASTNodePtr Parser::parseRepeatStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_REPEAT);
    std::vector<ASTNodePtr> stmts;
    while (!check(TokenKind::TOK_UNTIL) && !check(TokenKind::TOK_EOF)) {
        stmts.push_back(parseStatement());
        match(TokenKind::TOK_SEMI);
    }
    expect(TokenKind::TOK_UNTIL);
    auto cond = parseExpression();
    return std::make_unique<RepeatStmtNode>(std::move(stmts), std::move(cond), loc);
}

ASTNodePtr Parser::parseCaseStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_CASE);
    auto expr = parseExpression();
    expect(TokenKind::TOK_OF);

    std::vector<ASTNodePtr> arms;
    while (!check(TokenKind::TOK_END) && !check(TokenKind::TOK_EOF)) {
        SourceLoc armLoc = cur_.loc;
        if (cur_.kind != TokenKind::TOK_INTEGER_LIT)
            throw error("Expected integer constant in CASE arm");
        long long val = cur_.intVal;
        advance();
        expect(TokenKind::TOK_COLON);
        auto stmt = parseStatement();
        match(TokenKind::TOK_SEMI);
        arms.push_back(std::make_unique<CaseArmNode>(val, std::move(stmt), armLoc));
    }
    expect(TokenKind::TOK_END);
    return std::make_unique<CaseStmtNode>(std::move(expr), std::move(arms), loc);
}

ASTNodePtr Parser::parseProcCallStmt(std::string name, SourceLoc loc) {
    std::vector<ASTNodePtr> args;
    if (match(TokenKind::TOK_LPAREN)) {
        args = parseArgList();
        expect(TokenKind::TOK_RPAREN);
    }
    return std::make_unique<ProcCallStmtNode>(name, std::move(args), loc);
}

ASTNodePtr Parser::parseReturnStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_RETURN);
    ASTNodePtr val;
    if (match(TokenKind::TOK_LPAREN)) {
        val = parseExpression();
        expect(TokenKind::TOK_RPAREN);
    }
    return std::make_unique<ReturnStmtNode>(std::move(val), loc);
}

ASTNodePtr Parser::parseGotoStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_GOTO);
    std::string label = expectIdentifier();
    return std::make_unique<GotoStmtNode>(label, loc);
}

ASTNodePtr Parser::parseAsmStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_ASM);
    expect(TokenKind::TOK_LPAREN);
    if (cur_.kind != TokenKind::TOK_STRING_LIT)
        throw error("Expected string literal in ASM statement");
    std::string text = cur_.strVal;
    advance();
    expect(TokenKind::TOK_RPAREN);
    return std::make_unique<AsmStmtNode>(text, loc);
}

ASTNodePtr Parser::parsePrintStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_PRINT);
    expect(TokenKind::TOK_LPAREN);

    if (cur_.kind != TokenKind::TOK_STRING_LIT)
        throw error("First argument to PRINT must be a string literal");
    std::string fmt = cur_.strVal;
    advance();

    std::vector<ASTNodePtr> args;
    while (match(TokenKind::TOK_COMMA))
        args.push_back(parseExpression());

    expect(TokenKind::TOK_RPAREN);
    return std::make_unique<PrintStmtNode>(fmt, std::move(args), loc);
}

ASTNodePtr Parser::parsePushStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_PUSH);
    auto value = parseExpression();
    return std::make_unique<PushStmtNode>(std::move(value), loc);
}

ASTNodePtr Parser::parsePopStmt() {
    SourceLoc loc = cur_.loc;
    expect(TokenKind::TOK_POP);
    auto target = parseLValue();
    return std::make_unique<PopStmtNode>(std::move(target), loc);
}

// ============================================================
// Expressions
// ============================================================

ASTNodePtr Parser::parseExpression() { return parseRelExpr(); }

ASTNodePtr Parser::parseRelExpr() {
    auto left = parseAddExpr();
    while (isRelOp(cur_.kind)) {
        TokenKind op = cur_.kind;
        SourceLoc loc = cur_.loc;
        advance();
        auto right = parseAddExpr();
        left = std::make_unique<BinaryExprNode>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ASTNodePtr Parser::parseAddExpr() {
    auto left = parseMulExpr();
    while (isAddOp(cur_.kind)) {
        TokenKind op = cur_.kind;
        SourceLoc loc = cur_.loc;
        advance();
        auto right = parseMulExpr();
        left = std::make_unique<BinaryExprNode>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ASTNodePtr Parser::parseMulExpr() {
    auto left = parseUnaryExpr();
    while (isMulOp(cur_.kind)) {
        TokenKind op = cur_.kind;
        SourceLoc loc = cur_.loc;
        advance();
        auto right = parseUnaryExpr();
        left = std::make_unique<BinaryExprNode>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

ASTNodePtr Parser::parseUnaryExpr() {
    SourceLoc loc = cur_.loc;
    if (cur_.kind == TokenKind::TOK_MINUS) {
        advance();
        return std::make_unique<UnaryExprNode>(TokenKind::TOK_MINUS,
                                               parseUnaryExpr(), loc);
    }
    if (cur_.kind == TokenKind::TOK_NOT) {
        advance();
        return std::make_unique<UnaryExprNode>(TokenKind::TOK_NOT,
                                               parseUnaryExpr(), loc);
    }
    return parsePostfixExpr();
}

ASTNodePtr Parser::parsePostfixExpr() {
    auto expr = parsePrimaryExpr();
    SourceLoc loc = cur_.loc;

    while (true) {
        if (check(TokenKind::TOK_DEREF)) {
            advance();
            expr = std::make_unique<DerefExprNode>(std::move(expr), loc);
        } else if (check(TokenKind::TOK_LPAREN)) {
            // Function call or array index on a result
            advance();
            auto args = parseArgList();
            expect(TokenKind::TOK_RPAREN);
            expr = std::make_unique<IndexExprNode>(std::move(expr), std::move(args), loc);
        } else {
            break;
        }
    }
    return expr;
}

ASTNodePtr Parser::parsePrimaryExpr() {
    SourceLoc loc = cur_.loc;

    switch (cur_.kind) {
    case TokenKind::TOK_INTEGER_LIT: {
        long long v = cur_.intVal; advance();
        return std::make_unique<IntLiteralNode>(v, loc);
    }
    case TokenKind::TOK_REAL_LIT: {
        double v = cur_.realVal; advance();
        return std::make_unique<RealLiteralNode>(v, loc);
    }
    case TokenKind::TOK_STRING_LIT: {
        std::string v = cur_.strVal; advance();
        return std::make_unique<StringLiteralNode>(v, loc);
    }
    case TokenKind::TOK_BIT_LIT: {
        long long iv = cur_.intVal;
        std::string bs = cur_.strVal;
        advance();
        return std::make_unique<BitLiteralNode>(iv, bs, loc);
    }
    case TokenKind::TOK_ADDRESS_LIT: {
        long long addr = cur_.intVal; advance();
        return std::make_unique<AddressLiteralNode>(addr, loc);
    }
    case TokenKind::TOK_REGISTER: {
        int rn = cur_.regNum; advance();
        return std::make_unique<RegisterNode>(rn, loc);
    }
    case TokenKind::TOK_SP: {   // SP = R6 on PDP-11
        advance();
        return std::make_unique<RegisterNode>(6, loc);
    }
    case TokenKind::TOK_PC: {   // PC = R7 on PDP-11
        advance();
        return std::make_unique<RegisterNode>(7, loc);
    }
    case TokenKind::TOK_AT: {
        advance();
        // @variable — address-of
        auto var = parsePrimaryExpr();
        return std::make_unique<AddrOfExprNode>(std::move(var), loc);
    }
    case TokenKind::TOK_LPAREN: {
        advance();
        auto e = parseExpression();
        expect(TokenKind::TOK_RPAREN);
        return e;
    }
    case TokenKind::TOK_IDENTIFIER: {
        std::string name = expectIdentifier();
        // Function call?
        if (check(TokenKind::TOK_LPAREN)) {
            advance();
            std::vector<ASTNodePtr> args;
            if (!check(TokenKind::TOK_RPAREN))
                args = parseArgList();
            expect(TokenKind::TOK_RPAREN);
            return std::make_unique<FuncCallExprNode>(name, std::move(args), loc);
        }
        // Array element?
        auto id = std::make_unique<IdentifierNode>(name, loc);
        return id;
    }
    default:
        throw error("Expected expression, got '" + cur_.text + "'");
    }
}

std::vector<ASTNodePtr> Parser::parseArgList() {
    std::vector<ASTNodePtr> args;
    args.push_back(parseExpression());
    while (match(TokenKind::TOK_COMMA))
        args.push_back(parseExpression());
    return args;
}

} // namespace pl11
