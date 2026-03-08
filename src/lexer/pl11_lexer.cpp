#include "pl11_lexer.h"

#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace pl11 {

// ============================================================
// Keyword table (all upper-case; identifiers are uppercased before lookup)
// ============================================================

const std::unordered_map<std::string, TokenKind> Lexer::keywords_ = {
    {"AND",       TokenKind::TOK_AND},
    {"BYTE",      TokenKind::TOK_BYTE},
    {"ASM",       TokenKind::TOK_ASM},
    {"BEGIN",     TokenKind::TOK_BEGIN},
    {"BIT",       TokenKind::TOK_BIT},
    {"CALL",      TokenKind::TOK_CALL},
    {"CASE",      TokenKind::TOK_CASE},
    {"CHARACTER", TokenKind::TOK_CHARACTER},
    {"CONSTANT",  TokenKind::TOK_CONSTANT},
    {"DO",        TokenKind::TOK_DO},
    {"DOWNTO",    TokenKind::TOK_DOWNTO},
    {"ELSE",      TokenKind::TOK_ELSE},
    {"END",       TokenKind::TOK_END},
    {"FLOAT",     TokenKind::TOK_FLOAT},
    {"FOR",       TokenKind::TOK_FOR},
    {"FORWARD",   TokenKind::TOK_FORWARD},
    {"GOTO",      TokenKind::TOK_GOTO},
    {"IF",        TokenKind::TOK_IF},
    {"IN",        TokenKind::TOK_IN},
    {"INTEGER",   TokenKind::TOK_INTEGER},
    {"LONG",      TokenKind::TOK_LONG},
    {"MOD",       TokenKind::TOK_MOD},
    {"NOT",       TokenKind::TOK_NOT},
    {"OF",        TokenKind::TOK_OF},
    {"OR",        TokenKind::TOK_OR},
    {"OUT",       TokenKind::TOK_OUT},
    {"PC",        TokenKind::TOK_PC},
    {"PRINT",     TokenKind::TOK_PRINT},
    {"PROCEDURE", TokenKind::TOK_PROCEDURE},
    {"REAL",      TokenKind::TOK_REAL},
    {"REPEAT",    TokenKind::TOK_REPEAT},
    {"RETURN",    TokenKind::TOK_RETURN},
    {"SHL",       TokenKind::TOK_SHL},
    {"SHR",       TokenKind::TOK_SHR},
    {"SHRA",      TokenKind::TOK_SHRA},
    {"SP",        TokenKind::TOK_SP},
    {"THEN",      TokenKind::TOK_THEN},
    {"TO",        TokenKind::TOK_TO},
    {"UNTIL",     TokenKind::TOK_UNTIL},
    {"WHILE",     TokenKind::TOK_WHILE},
    {"WORD",      TokenKind::TOK_WORD},
    {"XOR",       TokenKind::TOK_XOR},
};

const char* tokenKindName(TokenKind k) {
    switch (k) {
#define CASE(x) case TokenKind::x: return #x
    CASE(TOK_INTEGER_LIT); CASE(TOK_REAL_LIT); CASE(TOK_STRING_LIT);
    CASE(TOK_BIT_LIT); CASE(TOK_ADDRESS_LIT); CASE(TOK_IDENTIFIER);
    CASE(TOK_AND); CASE(TOK_ASM); CASE(TOK_BEGIN); CASE(TOK_BIT); CASE(TOK_BYTE);
    CASE(TOK_CALL); CASE(TOK_CASE); CASE(TOK_CHARACTER); CASE(TOK_CONSTANT);
    CASE(TOK_DO); CASE(TOK_DOWNTO); CASE(TOK_ELSE); CASE(TOK_END);
    CASE(TOK_FLOAT); CASE(TOK_FOR); CASE(TOK_FORWARD); CASE(TOK_GOTO);
    CASE(TOK_IF); CASE(TOK_IN); CASE(TOK_INTEGER); CASE(TOK_LONG); CASE(TOK_MOD);
    CASE(TOK_NOT); CASE(TOK_OF); CASE(TOK_OR); CASE(TOK_OUT);
    CASE(TOK_PC); CASE(TOK_PRINT); CASE(TOK_PROCEDURE); CASE(TOK_REAL); CASE(TOK_REPEAT);
    CASE(TOK_RETURN); CASE(TOK_SHL); CASE(TOK_SHR); CASE(TOK_SHRA);
    CASE(TOK_SP); CASE(TOK_THEN); CASE(TOK_TO); CASE(TOK_UNTIL);
    CASE(TOK_WHILE); CASE(TOK_WORD); CASE(TOK_XOR); CASE(TOK_REGISTER);
    CASE(TOK_ASSIGN); CASE(TOK_EQ); CASE(TOK_NEQ); CASE(TOK_LT);
    CASE(TOK_GT); CASE(TOK_LEQ); CASE(TOK_GEQ); CASE(TOK_PLUS);
    CASE(TOK_MINUS); CASE(TOK_STAR); CASE(TOK_SLASH); CASE(TOK_AT);
    CASE(TOK_DEREF); CASE(TOK_LPAREN); CASE(TOK_RPAREN); CASE(TOK_SEMI);
    CASE(TOK_COMMA); CASE(TOK_COLON); CASE(TOK_DOT); CASE(TOK_EOF);
    CASE(TOK_ERROR);
#undef CASE
    default: return "<unknown>";
    }
}

// ============================================================
// Lexer implementation
// ============================================================

Lexer::Lexer(std::string source, std::string filename)
    : src_(std::move(source)), filename_(std::move(filename)) {}

char Lexer::current() const {
    return (pos_ < src_.size()) ? src_[pos_] : '\0';
}

char Lexer::peek1() const {
    return (pos_ + 1 < src_.size()) ? src_[pos_ + 1] : '\0';
}

void Lexer::advance() {
    if (pos_ < src_.size()) {
        if (src_[pos_] == '\n') { ++line_; col_ = 1; }
        else                    { ++col_; }
        ++pos_;
    }
}

SourceLoc Lexer::makeLoc() const {
    return {line_, col_, filename_};
}

Token Lexer::makeToken(TokenKind k, const std::string& text) {
    Token t;
    t.kind = k;
    t.text = text;
    t.loc  = makeLoc();
    return t;
}

Token Lexer::errorToken(const std::string& msg) {
    Token t;
    t.kind   = TokenKind::TOK_ERROR;
    t.text   = msg;
    t.loc    = makeLoc();
    t.strVal = msg;
    return t;
}

void Lexer::skipWhitespace() {
    while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(current())))
        advance();
}

// Skip (* ... *) comments (non-nested for simplicity; nesting is rare in PL-11 code)
void Lexer::skipComment() {
    // current() == '(' and peek1() == '*'
    advance(); advance();  // consume (*
    int depth = 1;
    while (pos_ < src_.size() && depth > 0) {
        if (current() == '(' && peek1() == '*') { advance(); advance(); ++depth; }
        else if (current() == '*' && peek1() == ')') { advance(); advance(); --depth; }
        else advance();
    }
}

Token Lexer::readIdentifierOrKeyword() {
    SourceLoc loc = makeLoc();
    std::string raw;
    while (pos_ < src_.size() &&
           (std::isalnum(static_cast<unsigned char>(current())) || current() == '_')) {
        raw += current();
        advance();
    }

    // Check for register: R followed by 1-2 digits
    std::string upper = raw;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper.size() >= 2 && upper[0] == 'R' && std::isdigit(static_cast<unsigned char>(upper[1]))) {
        bool allDigits = true;
        for (size_t i = 1; i < upper.size(); ++i)
            if (!std::isdigit(static_cast<unsigned char>(upper[i]))) { allDigits = false; break; }
        if (allDigits) {
            int regNum = std::stoi(upper.substr(1));
            if (regNum >= 0 && regNum <= 15) {
                Token t;
                t.kind   = TokenKind::TOK_REGISTER;
                t.text   = raw;
                t.loc    = loc;
                t.regNum = regNum;
                return t;
            }
        }
    }

    auto it = keywords_.find(upper);
    Token t;
    t.loc  = loc;
    t.text = raw;
    if (it != keywords_.end()) {
        t.kind = it->second;
    } else {
        t.kind   = TokenKind::TOK_IDENTIFIER;
        t.strVal = upper;  // normalize identifiers to upper case
    }
    return t;
}

Token Lexer::readNumber() {
    SourceLoc loc = makeLoc();
    std::string raw;

    // Optional leading minus (handled as unary in parser; lexer produces positive literals)
    bool isReal = false;

    // Hex literal: #digits
    if (current() == '#') {
        raw += current(); advance();
        while (pos_ < src_.size() && std::isxdigit(static_cast<unsigned char>(current()))) {
            raw += current(); advance();
        }
        Token t;
        t.kind   = TokenKind::TOK_INTEGER_LIT;
        t.text   = raw;
        t.loc    = loc;
        t.intVal = std::stoll(raw.substr(1), nullptr, 16);
        return t;
    }

    // Octal or decimal
    while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(current()))) {
        raw += current(); advance();
    }

    // Check for real: decimal point
    if (current() == '.' && peek1() != '.') {
        isReal = true;
        raw += current(); advance();
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(current()))) {
            raw += current(); advance();
        }
        if (current() == 'E' || current() == 'e') {
            raw += current(); advance();
            if (current() == '+' || current() == '-') { raw += current(); advance(); }
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(current()))) {
                raw += current(); advance();
            }
        }
    }

    // Check for 'B' suffix: address literal (octal address) or bit-string context
    bool isAddress = false;
    if (!isReal && (current() == 'B' || current() == 'b')) {
        isAddress = true;
        raw += current(); advance();
    }

    Token t;
    t.loc  = loc;
    t.text = raw;
    if (isReal) {
        t.kind    = TokenKind::TOK_REAL_LIT;
        t.realVal = std::stod(raw);
    } else if (isAddress) {
        t.kind   = TokenKind::TOK_ADDRESS_LIT;
        // raw ends with 'B'; parse the numeric part as octal
        std::string numPart = raw.substr(0, raw.size() - 1);
        t.intVal = std::stoll(numPart, nullptr,
                              (numPart.size() > 1 && numPart[0] == '0') ? 8 : 10);
    } else if (raw.size() > 1 && raw[0] == '0') {
        t.kind   = TokenKind::TOK_INTEGER_LIT;
        t.intVal = std::stoll(raw, nullptr, 8);
    } else {
        t.kind   = TokenKind::TOK_INTEGER_LIT;
        t.intVal = std::stoll(raw, nullptr, 10);
    }
    return t;
}

Token Lexer::readStringOrBitLiteral() {
    SourceLoc loc = makeLoc();
    advance();  // consume opening '
    std::string content;
    while (pos_ < src_.size()) {
        if (current() == '\'') {
            if (peek1() == '\'') {
                content += '\''; advance(); advance();  // '' → '
            } else {
                advance();  // consume closing '
                break;
            }
        } else if (current() == '\\') {
            // C-style escape sequences
            advance();
            switch (current()) {
            case 'n':  content += '\n'; break;
            case 't':  content += '\t'; break;
            case 'r':  content += '\r'; break;
            case '\\': content += '\\'; break;
            case '\'': content += '\''; break;
            case '0':  content += '\0'; break;
            default:   content += '\\'; content += current(); break;
            }
            advance();
        } else {
            content += current(); advance();
        }
    }

    // Check for 'B' suffix → bit literal
    if ((current() == 'B' || current() == 'b')) {
        bool allBits = true;
        for (char c : content) if (c != '0' && c != '1') { allBits = false; break; }
        if (allBits) {
            advance();  // consume B
            Token t;
            t.kind   = TokenKind::TOK_BIT_LIT;
            t.text   = "'" + content + "'B";
            t.loc    = loc;
            t.strVal = content;
            // Convert bit string to integer
            t.intVal = 0;
            for (char c : content) { t.intVal = (t.intVal << 1) | (c - '0'); }
            return t;
        }
    }

    Token t;
    t.kind   = TokenKind::TOK_STRING_LIT;
    t.text   = "'" + content + "'";
    t.loc    = loc;
    t.strVal = content;
    return t;
}

Token Lexer::readOperator() {
    SourceLoc loc = makeLoc();
    char c  = current();
    char c1 = peek1();

    auto tok = [&](TokenKind k, int len) {
        std::string text;
        for (int i = 0; i < len; ++i) { text += current(); advance(); }
        Token t;
        t.kind = k;
        t.text = text;
        t.loc  = loc;
        return t;
    };

    switch (c) {
    case ':':
        if (c1 == '=') return tok(TokenKind::TOK_ASSIGN, 2);
        return tok(TokenKind::TOK_COLON, 1);
    case '<':
        if (c1 == '>') return tok(TokenKind::TOK_NEQ, 2);
        if (c1 == '=') return tok(TokenKind::TOK_LEQ, 2);
        return tok(TokenKind::TOK_LT, 1);
    case '>':
        if (c1 == '=') return tok(TokenKind::TOK_GEQ, 2);
        return tok(TokenKind::TOK_GT, 1);
    case '=':  return tok(TokenKind::TOK_EQ,     1);
    case '+':  return tok(TokenKind::TOK_PLUS,   1);
    case '-':  return tok(TokenKind::TOK_MINUS,  1);
    case '*':  return tok(TokenKind::TOK_STAR,   1);
    case '/':  return tok(TokenKind::TOK_SLASH,  1);
    case '@':  return tok(TokenKind::TOK_AT,     1);
    case '^':  return tok(TokenKind::TOK_DEREF,  1);
    case '(':  return tok(TokenKind::TOK_LPAREN, 1);
    case ')':  return tok(TokenKind::TOK_RPAREN, 1);
    case ';':  return tok(TokenKind::TOK_SEMI,   1);
    case ',':  return tok(TokenKind::TOK_COMMA,  1);
    case '.':  return tok(TokenKind::TOK_DOT,    1);
    default:
        advance();
        return errorToken(std::string("Unexpected character: ") + c);
    }
}

Token Lexer::next() {
    if (hasPeek_) {
        hasPeek_ = false;
        return peekTok_;
    }

    // Skip whitespace and comments
    while (true) {
        skipWhitespace();
        if (current() == '(' && peek1() == '*') {
            skipComment();
        } else {
            break;
        }
    }

    if (pos_ >= src_.size()) {
        Token t;
        t.kind = TokenKind::TOK_EOF;
        t.text = "<EOF>";
        t.loc  = makeLoc();
        return t;
    }

    char c = current();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
        return readIdentifierOrKeyword();

    if (std::isdigit(static_cast<unsigned char>(c)) || c == '#')
        return readNumber();

    if (c == '\'')
        return readStringOrBitLiteral();

    return readOperator();
}

Token Lexer::peek() {
    if (!hasPeek_) {
        peekTok_ = next();
        hasPeek_ = true;
    }
    return peekTok_;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> toks;
    while (true) {
        Token t = next();
        toks.push_back(t);
        if (t.kind == TokenKind::TOK_EOF) break;
    }
    return toks;
}

} // namespace pl11
