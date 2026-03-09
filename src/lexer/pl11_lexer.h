#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace pl11 {

// ============================================================
// Token types
// ============================================================

enum class TokenKind {
    // Literals
    TOK_INTEGER_LIT,    // 42, 0177, #FF
    TOK_REAL_LIT,       // 3.14, 2.0E-3
    TOK_STRING_LIT,     // 'HELLO'
    TOK_BIT_LIT,        // '101010'B
    TOK_ADDRESS_LIT,    // 0177560B (octal address)

    // Identifiers and keywords
    TOK_IDENTIFIER,

    // Keywords
    TOK_AND,
    TOK_ARRAY,
    TOK_ASM,
    TOK_BEGIN,
    TOK_BIT,
    TOK_BYTE,
    TOK_CALL,
    TOK_CASE,
    TOK_CHARACTER,
    TOK_CONSTANT,
    TOK_DO,
    TOK_DOWNTO,
    TOK_ELSE,
    TOK_END,
    TOK_FLOAT,
    TOK_FOR,
    TOK_FORWARD,
    TOK_FROM,
    TOK_GOTO,
    TOK_IF,
    TOK_IN,
    TOK_INTEGER,    // kept as alias for WORD
    TOK_LONG,
    TOK_MOD,
    TOK_NOT,
    TOK_OF,
    TOK_OR,
    TOK_OUT,
    TOK_PC,
    TOK_POP,
    TOK_PRINT,
    TOK_PUSH,
    TOK_PROCEDURE,
    TOK_REAL,
    TOK_REPEAT,
    TOK_RETURN,
    TOK_SHL,
    TOK_SHR,
    TOK_SHRA,
    TOK_SP,
    TOK_STEP,
    TOK_THEN,
    TOK_TO,
    TOK_UNTIL,
    TOK_WHILE,
    TOK_WORD,
    TOK_XOR,

    // Register names R0-R15
    TOK_REGISTER,       // value stored in Token::regNum

    // Operators
    TOK_ARROW,          // =>  (UNH: value => lvalue assignment)
    TOK_EQ,             // =
    TOK_NEQ,            // /=
    TOK_REF,            // REF  (UNH: address-of)
    TOK_IND,            // IND  (UNH: indirect / dereference)
    TOK_LT,             // <
    TOK_GT,             // >
    TOK_LEQ,            // <=
    TOK_GEQ,            // >=
    TOK_PLUS,           // +
    TOK_MINUS,          // -
    TOK_STAR,           // *
    TOK_SLASH,          // /
    TOK_DEREF,          // ^

    // Punctuation
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_SEMI,           // ;
    TOK_COMMA,          // ,
    TOK_COLON,          // :
    TOK_DOT,            // .

    // Special
    TOK_EOF,
    TOK_ERROR,
};

const char* tokenKindName(TokenKind k);

// ============================================================
// Source location
// ============================================================

struct SourceLoc {
    int line;
    int col;
    std::string file;
};

// ============================================================
// Token
// ============================================================

struct Token {
    TokenKind    kind;
    std::string  text;    // raw source text
    SourceLoc    loc;

    // Decoded literal values (set by lexer)
    long long    intVal    = 0;
    double       realVal   = 0.0;
    std::string  strVal;   // string/bit literal contents
    int          regNum    = -1;  // register number for TOK_REGISTER
};

// ============================================================
// Lexer
// ============================================================

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");

    // Return next token (advances position).
    Token next();

    // Peek at the next token without consuming it.
    Token peek();

    // Tokenize entire input into a vector.
    std::vector<Token> tokenize();

private:
    std::string src_;
    std::string filename_;
    size_t      pos_  = 0;
    int         line_ = 1;
    int         col_  = 1;

    // Lookahead token cache
    bool        hasPeek_ = false;
    Token       peekTok_;

    char current() const;
    char peek1() const;
    void advance();
    void skipWhitespace();
    void skipLineComment();       // % ... <eol>
    void skipCommentStatement();  // COMMENT ... ;
    bool isCommentKeyword() const;
    SourceLoc makeLoc() const;

    Token readIdentifierOrKeyword();
    Token readNumber();
    Token readStringOrBitLiteral();
    Token readOperator();
    Token makeToken(TokenKind k, const std::string& text);
    Token errorToken(const std::string& msg);

    static const std::unordered_map<std::string, TokenKind> keywords_;
};

} // namespace pl11
