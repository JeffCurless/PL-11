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
    | arrayDecl
    | constDecl
    | procDecl
    | forwardDecl
    ;

// UNH alternative: ARRAY size[, size ...] type name[, name ...]
arrayDecl
    : ARRAY intLiteral (COMMA intLiteral)* typeSpec IDENTIFIER (COMMA IDENTIFIER)*
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
    | doStmt
    | repeatStmt
    | caseStmt
    | procCallStmt
    | returnStmt
    | gotoStmt
    | labelStmt
    | asmStmt
    | printStmt
    | pushStmt
    | popStmt
    | block
    ;

assignment
    : expression ARROW variable       // UNH form: value => target
    | variable ASSIGN_OP expression   // traditional form: target := value
    ;

ifStmt
    : IF expression THEN statement (ELSE statement)?
    ;

whileStmt
    : WHILE expression DO statement (UNTIL expression)?
    ;

forStmt
    : FOR IDENTIFIER forInit (TO | DOWNTO) expression
      DO statement (UNTIL expression)?
    ;

// Four header forms for the loop variable initialiser:
//   FOR i := start            — traditional assignment
//   FOR i FROM start [STEP n] — UNH explicit start with optional step
//   FOR i STEP n              — UNH no init, explicit step (use current value)
//   FOR i                     — UNH no init, default step ±1 (use current value)
forInit
    : ASSIGN_OP expression                           // FOR i := start
    | FROM expression (STEP expression)?             // FOR i FROM start [STEP n]
    | STEP expression                                // FOR i STEP n  (current value)
    |                                                // FOR i         (current value, default step)
    ;

doStmt
    : DO (SEMI | statement) (UNTIL expression)?
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

// PUSH / POP — PDP-11 stack operations (UNH extension)
// PUSH pre-decrements SP by 2 then stores the value.
// POP loads from SP then post-increments SP by 2.
pushStmt
    : PUSH expression
    ;

popStmt
    : POP variable
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
    : EQ | NEQ | NEQ_ALT | LT | GT | LEQ | GEQ
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
ARRAY       : 'ARRAY' ;
FROM        : 'FROM' ;
STEP        : 'STEP' ;
SP          : 'SP' ;
PC          : 'PC' ;
PRINT       : 'PRINT' ;
POP         : 'POP' ;
PUSH        : 'PUSH' ;

// Operators and punctuation
ASSIGN_OP   : ':=' ;
ARROW       : '=>' ;   // UNH: value => lvalue assignment
ASSIGN      : '=' ;
EQ          : '=' ;
NEQ         : '<>' ;   // CERN form — synonym for /=
NEQ_ALT     : '/=' ;   // UNH form — preferred not-equal operator
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

// Comments — three forms, all skipped:
//   (* ... *)    Block comment. ANTLR does not natively support nesting;
//                for production use, implement a custom lexer action.
BLOCK_COMMENT
    : '(*' .*? '*)' -> skip
    ;

//   % ...        Line comment — extends to end of line.
//                (% inside a STRING_LITERAL is not a comment; that case
//                 must be handled before this rule fires.)
LINE_COMMENT
    : '%' ~[\r\n]* -> skip
    ;

//   COMMENT ... ; Keyword comment — extends to next semicolon.
//                ANTLR rule: match the word COMMENT then consume to ';'.
KEYWORD_COMMENT
    : 'COMMENT' .*? ';' -> skip
    ;

// Whitespace
WS
    : [ \t\r\n]+ -> skip
    ;
