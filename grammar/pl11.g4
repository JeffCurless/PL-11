// PL-11 ANTLR4 Grammar
// Based on CERN Technical Report CERN-74-24, R.D. Russell, 1974
// For use with ANTLR4 (https://www.antlr.org/)
//
// Usage:
//   antlr4 -Dlanguage=Cpp pl11.g4
//   or for parse-tree testing:
//   antlr4 pl11.g4 -o gen/
//   javac -cp antlr-4.x-complete.jar gen/*.java
//   grun pl11 program -tree < hello.pl11

grammar pl11;

options {
    caseInsensitive = true;
}

// ============================================================
// Parser Rules
// ============================================================

program
    : block EOF
    ;

block
    : BEGIN declSection stmtList END
    ;

declSection
    : (declaration SEMI)*
    ;

stmtList
    : (statement SEMI)* statement?
    ;

// ---- Declarations ----

declaration
    : varDecl
    | constDecl
    | procDecl
    | forwardDecl
    ;

varDecl
    : typeSpec varDeclarator (COMMA varDeclarator)*
    ;

varDeclarator
    : IDENTIFIER (LPAREN dimList RPAREN)?
    ;

dimList
    : expression (COMMA expression)*
    ;

typeSpec
    : BYTE                         // 8-bit signed integer
    | WORD                         // 16-bit signed integer
    | LONG                         // 32-bit signed integer
    | INTEGER                      // alias for WORD
    | REAL    (STAR intLiteral)?
    | CHARACTER (STAR intLiteral)?
    | BIT     (STAR intLiteral)?
    ;

constDecl
    : CONSTANT typeSpec IDENTIFIER ASSIGN literal
    ;

procDecl
    : typeSpec? PROCEDURE IDENTIFIER (LPAREN paramList RPAREN)? SEMI
      block SEMI
    ;

forwardDecl
    : PROCEDURE FORWARD IDENTIFIER SEMI
    ;

paramList
    : paramDecl (SEMI paramDecl)*
    ;

paramDecl
    : paramMode typeSpec IDENTIFIER (COMMA IDENTIFIER)*
    ;

paramMode
    : IN OUT
    | IN
    | OUT
    |       // empty — defaults to IN
    ;

// ---- Statements ----

statement
    : assignment
    | ifStmt
    | whileStmt
    | forStmt
    | repeatStmt
    | caseStmt
    | procCallStmt
    | returnStmt
    | gotoStmt
    | labelStmt
    | asmStmt
    | printStmt
    | block
    ;

assignment
    : variable ASSIGN_OP expression
    ;

ifStmt
    : IF expression THEN statement (ELSE statement)?
    ;

whileStmt
    : WHILE expression DO statement (UNTIL expression)?
    ;

forStmt
    : FOR IDENTIFIER ASSIGN_OP expression (TO | DOWNTO) expression
      DO statement (UNTIL expression)?
    ;

repeatStmt
    : REPEAT stmtList UNTIL expression
    ;

caseStmt
    : CASE expression OF caseArm* END
    ;

caseArm
    : intLiteral COLON statement SEMI
    ;

procCallStmt
    : CALL IDENTIFIER (LPAREN argList RPAREN)?
    ;

returnStmt
    : RETURN (LPAREN expression RPAREN)?
    ;

gotoStmt
    : GOTO IDENTIFIER
    ;

labelStmt
    : IDENTIFIER COLON statement
    ;

asmStmt
    : ASM LPAREN STRING_LITERAL RPAREN
    ;

// PRINT: built-in formatted console output (maps to C printf)
// Specifiers: %d=WORD/BYTE  %l=LONG  %f=REAL  %c=char  %s=CHARACTER  %%=literal
printStmt
    : PRINT LPAREN STRING_LITERAL (COMMA expression)* RPAREN
    ;

argList
    : expression (COMMA expression)*
    ;

// ---- Expressions (operator precedence via hierarchy) ----

expression
    : relExpr
    ;

relExpr
    : addExpr (relOp addExpr)*
    ;

addExpr
    : mulExpr (addOp mulExpr)*
    ;

mulExpr
    : unaryExpr (mulOp unaryExpr)*
    ;

unaryExpr
    : MINUS unaryExpr
    | NOT   unaryExpr
    | postfixExpr
    ;

postfixExpr
    : primaryExpr (DEREF | LPAREN argList RPAREN)*
    ;

primaryExpr
    : literal
    | AT variable
    | functionCall
    | variable
    | LPAREN expression RPAREN
    ;

functionCall
    : IDENTIFIER LPAREN argList RPAREN
    ;

variable
    : IDENTIFIER (LPAREN indexList RPAREN)?
    | registerName
    ;

indexList
    : expression (COMMA expression)*
    ;

// ---- Operators ----

relOp
    : EQ | NEQ | LT | GT | LEQ | GEQ
    ;

addOp
    : PLUS | MINUS | OR | XOR
    ;

mulOp
    : STAR | SLASH | MOD | AND | SHL | SHR | SHRA
    ;

// ---- Literals ----

literal
    : intLiteral
    | realLiteral
    | STRING_LITERAL
    | BIT_LITERAL
    | ADDRESS_LITERAL
    ;

intLiteral
    : DECIMAL_LITERAL
    | OCTAL_LITERAL
    | HEX_LITERAL
    ;

realLiteral
    : REAL_LITERAL
    ;

registerName
    : REGISTER
    | SP
    | PC
    ;

// ============================================================
// Lexer Rules
// ============================================================

// Keywords
BEGIN       : 'BEGIN' ;
BYTE        : 'BYTE' ;
END         : 'END' ;
IF          : 'IF' ;
THEN        : 'THEN' ;
ELSE        : 'ELSE' ;
WHILE       : 'WHILE' ;
WORD        : 'WORD' ;
DO          : 'DO' ;
FOR         : 'FOR' ;
TO          : 'TO' ;
DOWNTO      : 'DOWNTO' ;
REPEAT      : 'REPEAT' ;
UNTIL       : 'UNTIL' ;
CASE        : 'CASE' ;
OF          : 'OF' ;
CALL        : 'CALL' ;
PROCEDURE   : 'PROCEDURE' ;
FORWARD     : 'FORWARD' ;
RETURN      : 'RETURN' ;
GOTO        : 'GOTO' ;
ASM         : 'ASM' ;
IN          : 'IN' ;
OUT         : 'OUT' ;
CONSTANT    : 'CONSTANT' ;
INTEGER     : 'INTEGER' ;  // alias for WORD
LONG        : 'LONG' ;
REAL        : 'REAL' | 'FLOAT' ;
CHARACTER   : 'CHARACTER' ;
BIT         : 'BIT' ;
AND         : 'AND' ;
OR          : 'OR' ;
NOT         : 'NOT' ;
XOR         : 'XOR' ;
MOD         : 'MOD' ;
SHL         : 'SHL' ;
SHR         : 'SHR' ;
SHRA        : 'SHRA' ;
SP          : 'SP' ;
PC          : 'PC' ;
PRINT       : 'PRINT' ;

// Operators and punctuation
ASSIGN_OP   : ':=' ;
ASSIGN      : '=' ;
EQ          : '=' ;
NEQ         : '<>' ;
LEQ         : '<=' ;
GEQ         : '>=' ;
LT          : '<' ;
GT          : '>' ;
PLUS        : '+' ;
MINUS       : '-' ;
STAR        : '*' ;
SLASH       : '/' ;
AT          : '@' ;
DEREF       : '^' ;
LPAREN      : '(' ;
RPAREN      : ')' ;
SEMI        : ';' ;
COMMA       : ',' ;
COLON       : ':' ;
DOT         : '.' ;

// Registers: R0-R15
REGISTER
    : 'R' [0-9] [0-9]?
    ;

// Literals
DECIMAL_LITERAL
    : '-'? [1-9] [0-9]*
    | '0'
    ;

OCTAL_LITERAL
    : '0' [0-7]+
    ;

HEX_LITERAL
    : '#' [0-9A-Fa-f]+
    ;

REAL_LITERAL
    : '-'? [0-9]+ '.' [0-9]* ([Ee] [+-]? [0-9]+)?
    ;

// Bit literal: '0101...'B
BIT_LITERAL
    : '\'' [01]+ '\'' 'B'
    ;

// Address literal: octal number followed by B
ADDRESS_LITERAL
    : '0' [0-7]+ 'B'
    ;

// String literal: 'text' (single quotes; '' for embedded quote)
STRING_LITERAL
    : '\'' ('\'\'' | ~['\r\n])* '\''
    ;

// Identifiers (case-insensitive via grammar option)
IDENTIFIER
    : [A-Za-z] [A-Za-z0-9_]*
    ;

// Comments: (* ... *) — ANTLR does not natively support nested comments;
// for production use, implement a custom lexer action for nesting.
COMMENT
    : '(*' .*? '*)' -> skip
    ;

// Whitespace
WS
    : [ \t\r\n]+ -> skip
    ;
