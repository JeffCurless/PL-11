# PL-11 Language Reference

**PL-11: A Programming Language for the DEC PDP-11 Computer**
Based on CERN Technical Report CERN-74-24, R.D. Russell, 1974.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Program Structure](#2-program-structure)
3. [Data Types](#3-data-types)
4. [Declarations](#4-declarations)
5. [Operators](#5-operators)
6. [Control Structures](#6-control-structures)
7. [Procedures](#7-procedures)
8. [Hardware and Machine Access](#8-hardware-and-machine-access)
9. [Scope Rules](#9-scope-rules)
10. [Sample Programs](#10-sample-programs)

---

## 1. Introduction

### 1.1 History and Context

PL-11 was designed by R.D. Russell at CERN (the European Organization for Nuclear Research) in 1971 and formally documented in 1974 as CERN Technical Report CERN-74-24. It targets the DEC PDP-11 minicomputer, a 16-bit word-addressed machine that was widely used in scientific computing, laboratory control, and real-time data acquisition during the 1970s.

The PDP-11 was notable for its orthogonal instruction set, memory-mapped I/O, and a uniform register model that made it an excellent target for systems programming. PL-11 was designed to give programmers direct access to these machine features while retaining the readability benefits of a high-level language.

### 1.2 Design Goals

PL-11 was designed with three primary goals:

1. **Machine orientation** — provide direct access to PDP-11 registers, memory addresses, and word sizes, enabling systems and real-time programming without assembly language.
2. **Structured programming** — follow the ALGOL-60 tradition of block-structured, lexically scoped programs with explicit control flow.
3. **Simplicity** — a small, learnable language with clear semantics, suitable for scientific computing staff who were not professional programmers.

### 1.3 Relationship to PL/360 and ALGOL-60

PL-11 belongs to the **PL/360 family** of languages, pioneered by Niklaus Wirth (designer of Pascal and Modula-2) for the IBM System/360. The PL/360 design philosophy is:

- ALGOL-60 block structure and scoping
- Machine-level data types (words, bytes, bits) matching the target architecture
- Direct register and address manipulation
- No automatic storage management (no garbage collection)

PL-11 adapts this philosophy to the PDP-11. Key similarities with ALGOL-60 include:

- `BEGIN`-`END` blocks as the primary structuring mechanism
- Lexical scoping of variables
- Procedure declarations with parameter passing modes
- `IF-THEN-ELSE`, `WHILE`, and `FOR` control structures

PL-11 extends ALGOL-60 with machine-oriented features: explicit word sizes in type declarations, register variables (R0-R15), and direct memory addressing.

---

## 2. Program Structure

A PL-11 program is a single **block**. A block consists of an optional declaration section followed by executable statements, wrapped in `BEGIN` and `END` delimiters.

```
BEGIN
    declarations
    statements
END
```

### 2.1 Blocks

Blocks are the fundamental unit of program structure. They can appear:

- As the top-level program
- As the body of a procedure
- As the body of a control structure (`IF`, `WHILE`, `FOR`)
- Anywhere a statement is expected (nested blocks)

Declarations within a block are local to that block. When a block is entered, local variables are allocated; when the block exits, they are deallocated.

### 2.2 Statement Termination

Statements are separated by semicolons (`;`). The last statement before `END` does not require a trailing semicolon, but one is permitted.

### 2.3 Comments

PL-11 supports three comment forms. All are stripped before parsing and produce no tokens.

#### Block comments — `(* … *)`

Begin with `(*` and end with `*)`. May span multiple lines and may be nested:

```
(* This is a comment *)

(*
   Multi-line comment.
   (* Nested block comments are allowed. *)
*)
```

#### Line comments — `%`

A `%` character that is not inside a string literal starts a comment that extends to the end of the line:

```
WORD COUNT;      % number of items processed
I := I + 1;      % advance to next element
```

The `%` sign inside a quoted string is treated literally and does not start a comment:

```
PRINT('100%% complete\n')   % the %% in the string is not a comment
```

#### Keyword comments — `COMMENT … ;`

The word `COMMENT` (case-insensitive) starts a comment that extends to the next semicolon. This form may span multiple lines:

```
COMMENT Set up initial values for the main loop;

COMMENT
    This block initialises the hardware interface.
    It must be called once before entering the event loop.
;
```

The terminating `;` is consumed as part of the comment and does not act as a statement separator.

### 2.4 Minimal Program

```
BEGIN
    (* Minimal PL-11 program — does nothing *)
END
```

### 2.5 A Simple Program

```
BEGIN
    WORD X, Y, SUM;
    X := 10;
    Y := 32;
    SUM := X + Y
END
```

---

## 3. Data Types

PL-11 provides four primitive types, all corresponding directly to PDP-11 hardware representations.

### 3.1 Integer Types

PL-11 provides three named integer types, each corresponding to a specific PDP-11 storage width:

| Type     | Size    | Range                              |
|----------|---------|------------------------------------|
| `BYTE`   | 8 bits  | -128 to 127                        |
| `WORD`   | 16 bits | -32768 to 32767                    |
| `LONG`   | 32 bits | -2,147,483,648 to 2,147,483,647    |

`WORD` is the native PDP-11 integer size and the most common type. `INTEGER` is accepted as an alias for `WORD` for readability.

```
WORD  I, J, K;
BYTE  CH;
LONG  BIGVAL;
```

Integer literals are written as decimal numerals (`0`, `42`, `-7`) or as octal with a leading zero (`0177777`).

### 3.2 REAL

`REAL` (sometimes written `FLOAT`) represents a floating-point number in PDP-11 native floating-point format (F-format, 32-bit, or D-format, 64-bit depending on the FPU option fitted).

```
REAL X, Y;
REAL*8 DPREC;    (* double precision *)
```

Real literals use a decimal point and optional exponent: `3.14`, `2.0E-3`, `-1.5E10`.

### 3.3 CHARACTER

`CHARACTER` represents a string of ASCII characters. A `CHARACTER` variable holds a fixed-length string; the length is specified at declaration time.

```
CHARACTER*20 NAME;
CHARACTER*1  CH;
```

String literals are enclosed in single quotes: `'HELLO'`, `'A'`.

The length of a `CHARACTER` variable is always fixed. Assignment pads with spaces on the right if the source is shorter, and truncates on the right if longer.

### 3.4 BIT

`BIT` represents a fixed-length bit string used for hardware-level manipulation. Bit strings are used for flag registers, device status words, and similar machine-level data.

```
BIT*16 FLAGS;
BIT*8  MASK;
```

Bit literals use single-quoted strings of `0` and `1` characters, or hexadecimal with a leading `#`:

```
FLAGS := '1010000000000011'B;    (* binary literal *)
MASK  := #FF;                    (* hex literal *)
```

Bitwise operations (`AND`, `OR`, `NOT`, `XOR`) and shift operations (`SHL`, `SHR`) apply to `BIT` operands.

### 3.5 Address and Pointer Types

PL-11 allows declaring variables as **addresses** (pointers) to other values. The address of a variable can be taken, and addresses can be dereferenced:

```
WORD ARRAY(100);     (* array of 100 integers *)
WORD   PTR;            (* can hold an address *)

PTR := @ARRAY;            (* @ takes address of ARRAY *)
PTR^ := 42;               (* ^ dereferences PTR *)
```

Direct numeric addresses can be assigned to pointer variables for memory-mapped I/O:

```
WORD STATUSREG;
STATUSREG := 177560B;     (* PDP-11 octal address of console status *)
```

---

## 4. Declarations

### 4.1 Variable Declarations

Variables are declared at the beginning of a block, before any executable statements. The syntax is:

```
type_spec variable_name { , variable_name } ;
```

Examples:

```
WORD I, J, K;
REAL    X, Y, Z;
CHARACTER*10 FIRST, LAST;
BIT*16  STATUS;
```

### 4.2 Array Declarations

PL-11 supports two syntaxes for declaring arrays. Both are equivalent and may be freely mixed in the same program.

#### Traditional syntax — `type name(size)`

Dimensions are given in parentheses after the variable name:

```
WORD SCORES(50);            (* array of 50 integers, indexed 1..50 *)
REAL MATRIX(10, 10);        (* 10×10 array of reals *)
CHARACTER*8 NAMES(25);      (* 25 strings of length 8 *)
```

#### ARRAY syntax — `ARRAY size type name`

An alternative form places the size before the type and name. This reads naturally as "an array of N elements of type T":

```
ARRAY 50   WORD    SCORES;         (* same as WORD SCORES(50) *)
ARRAY 3, 4 REAL    MATRIX;         (* same as REAL MATRIX(3, 4) *)
ARRAY 512  BYTE    BUFFER;         (* 512-byte I/O buffer *)
```

For multi-dimensional arrays, comma-separate the dimensions:

```
ARRAY 10, 10 WORD TABLE;           (* 10×10 table *)
```

Multiple names sharing the same size and type can be declared on one line:

```
ARRAY 4 WORD ROW, COL;             (* two separate arrays of 4 words each *)
```

#### Indexing

Array indexing uses parentheses: `SCORES(1)`, `MATRIX(I, J)`.

All PL-11 arrays are **1-based**: the first element has index 1, the last has index equal to the declared size.

Multi-dimensional arrays are stored in row-major order and indexed with comma-separated subscripts: `MATRIX(I, J)`. Internally the layout is flat — `MATRIX(I, J)` is equivalent to element `(I-1)*cols + J`.

### 4.3 Constants

Named constants are declared with `CONSTANT`:

```
CONSTANT WORD MAXN = 100;
CONSTANT REAL    PI   = 3.14159265;
```

Constants cannot be assigned to; they are substituted at compile time.

### 4.4 Declaration Order

All declarations in a block must precede all executable statements. The compiler reads the entire declaration section before processing statements. Declarations within a block are mutually visible (order does not matter within the same block).

---

## 5. Operators

### 5.1 Arithmetic Operators

| Operator | Meaning          | Operand Types        |
|----------|------------------|----------------------|
| `+`      | Addition         | WORD, REAL        |
| `-`      | Subtraction      | WORD, REAL        |
| `*`      | Multiplication   | WORD, REAL        |
| `/`      | Division         | WORD (truncates), REAL |
| `MOD`    | Modulus (remainder) | WORD           |
| `-`      | Unary negation   | WORD, REAL        |

Integer division truncates toward zero. `MOD` returns the remainder with the sign of the dividend.

### 5.2 Relational Operators

Relational operators produce a boolean result used in conditions. PL-11 has no separate boolean type; a zero value is false, any non-zero value is true.

| Operator | Meaning                  |
|----------|--------------------------|
| `=`      | Equal                    |
| `/=`     | Not equal                |
| `<>`     | Not equal (synonym for `/=`) |
| `<`      | Less than                |
| `>`      | Greater than             |
| `<=`     | Less than or equal       |
| `>=`     | Greater than or equal    |

`/=` is the canonical not-equal operator in the UNH dialect; `<>` is retained as a synonym for compatibility. Both produce identical code.

### 5.3 Logical and Bitwise Operators

These operators work on `BIT` operands at the bit level, and on integer conditions as boolean operators.

| Operator | Meaning             | Operand Types   |
|----------|---------------------|-----------------|
| `AND`    | Bitwise/logical AND | BIT, WORD    |
| `OR`     | Bitwise/logical OR  | BIT, WORD    |
| `NOT`    | Bitwise/logical NOT | BIT, WORD    |
| `XOR`    | Exclusive OR        | BIT, WORD    |
| `SHL`    | Shift left          | BIT, WORD    |
| `SHR`    | Shift right (logical) | BIT, WORD  |
| `SHRA`   | Shift right (arithmetic) | WORD    |

Shift amounts are given as: `FLAGS SHL 3`.

### 5.4 Assignment Operators

PL-11 supports two assignment forms. Both are **statements** — assignment cannot appear nested inside an expression.

#### `=>` — UNH form (value on left, target on right)

```
10        => X;          (* assign 10 to X              *)
A + B     => SUM;        (* assign sum to SUM           *)
3.14159   => PI;         (* assign real literal to PI   *)
-1        => R0;         (* assign -1 to register R0    *)
R0        => R1;         (* copy register R0 to R1      *)
100       => VALS(1);    (* assign to array element     *)
```

The value expression (left of `=>`) may be any expression. The target (right of `=>`) must be a variable, array element, or register name — not an arbitrary expression.

#### `:=` — Traditional form (target on left, value on right)

```
X := Y + Z * 2;
```

The traditional ALGOL-60 form is fully supported alongside `=>`. Both forms produce identical code; either may be used freely within the same program.

### 5.5 Operator Precedence

From highest (evaluated first) to lowest:

| Precedence | Operators                          |
|------------|------------------------------------|
| 1 (highest)| Unary `-`, `NOT`                   |
| 2          | `*`, `/`, `MOD`, `AND`, `SHL`, `SHR`, `SHRA` |
| 3          | `+`, `-`, `OR`, `XOR`             |
| 4 (lowest) | `=`, `<>`, `<`, `>`, `<=`, `>=`  |

Parentheses can override precedence: `(A + B) * C`.

---

## 6. Control Structures

### 6.1 IF-THEN-ELSE

```
IF condition THEN statement
IF condition THEN statement ELSE statement
```

The condition is any expression (zero = false, non-zero = true). The `ELSE` clause is optional. To execute multiple statements in a branch, use a `BEGIN`-`END` block:

```
IF X > 0 THEN
    BEGIN
        Y := X * 2;
        Z := Y - 1
    END
ELSE
    Z := 0
```

Dangling `ELSE` is resolved by associating `ELSE` with the nearest preceding `IF` (standard rule).

### 6.2 WHILE

```
WHILE condition DO statement
WHILE condition DO statement UNTIL break_condition
```

Executes `statement` repeatedly as long as `condition` is non-zero. The condition is tested before each iteration (pre-test loop).

The optional `UNTIL` clause provides an early-exit test evaluated **after** each iteration of the body. If `break_condition` is non-zero, the loop exits immediately — equivalent to an inline `IF break_condition THEN BREAK`. When no UNTIL clause is present, behaviour is unchanged.

Control flow with UNTIL:
```
→ while.cond:  if !condition  → while.exit
→ while.body:  [loop body]
→ while.until: if break_cond → while.exit
→              else          → while.cond
→ while.exit:
```

Examples:

```
(* Simple accumulator — no UNTIL *)
WHILE I <= N DO
    BEGIN
        SUM := SUM + A(I);
        I := I + 1
    END

(* Search loop — exits when element found or end reached *)
WHILE I <= N DO
    BEGIN
        I := I + 1
    END
UNTIL A(I) = KEY
```

### 6.3 FOR

PL-11 supports two equivalent syntaxes for the FOR loop header.

#### Traditional syntax

```
FOR variable := start TO     end DO statement [ UNTIL break_condition ]
FOR variable := start DOWNTO end DO statement [ UNTIL break_condition ]
```

#### UNH syntax — `FROM`, optional `STEP`, optional start

The UNH extensions add two new keywords and make the start-value assignment optional:

```
FOR variable FROM start [STEP step] TO     end DO statement [ UNTIL break_condition ]
FOR variable FROM start [STEP step] DOWNTO end DO statement [ UNTIL break_condition ]
FOR variable          [STEP step] TO     end DO statement [ UNTIL break_condition ]
FOR variable          [STEP step] DOWNTO end DO statement [ UNTIL break_condition ]
```

**`FROM start`** — replaces `:=` as the separator between the loop variable and its initial value. When `FROM` (and `:=`) are both omitted, the loop begins at the variable's **current value** — no initialisation is performed.

**`STEP step_expr`** — provides an explicit per-iteration increment. When omitted, the default is `+1` for `TO` and `−1` for `DOWNTO`. The step expression is evaluated once before the loop begins. It may be any arithmetic expression, including a negative literal.

`TO`/`DOWNTO` control the **termination condition** (`i ≤ end` / `i ≥ end`) independently of the step sign.

Control flow:
```
→ [store start → var]     ← only when a start value is given
→ for.cond:  load var; if var > end (or var < end for DOWNTO) → for.exit
→ for.body:  [loop body]
→ for.until: if break_cond → for.exit    ← only when UNTIL present
→            else          → for.step
→ for.step:  var := var + step  →  for.cond
→ for.exit:
```

Examples:

```
(* Traditional := syntax — unchanged *)
FOR I := 1 TO N DO
    SUM := SUM + A(I);

(* FROM syntax — equivalent to above *)
FOR I FROM 1 TO N DO
    SUM := SUM + A(I);

(* Count down by two: 10, 8, 6, 4, 2 *)
FOR I FROM 10 STEP -2 DOWNTO 2 DO
    PRINT('%d\n', I);

(* No start: continue from current I, step up to 10 *)
I := 3;
FOR I TO 10 DO
    SUM := SUM + I;   (* adds 3 + 4 + 5 + … + 10 *)

(* No start, explicit negative step: count down from current I to 1 *)
I := 5;
FOR I STEP -1 DOWNTO 1 DO
    PROCESS(I);       (* visits 5, 4, 3, 2, 1 *)

(* No start + STEP + UNTIL: scan from current position until condition *)
FOR I STEP 2 TO 100 DO
    SUM := SUM + I
UNTIL SUM > 50;

(* Sum odd numbers with early exit *)
FOR I FROM 1 STEP 2 TO 100 DO
    SUM := SUM + I
UNTIL SUM > 20;
```

The loop variable retains its exit value after the loop (useful with `UNTIL` to know which iteration triggered the exit). The loop variable must not be modified inside the loop body.

### 6.4 CASE

```
CASE expression OF
    constant1 : statement1 ;
    constant2 : statement2 ;
    ...
    constantN : statementN
END
```

`CASE` evaluates `expression` (must be `WORD`) and executes the matching branch. If no case matches, the behavior is undefined (no default case in standard PL-11; use an `IF` before the `CASE` to guard against out-of-range values).

```
CASE CODE OF
    1 : CALL READ_DATA;
    2 : CALL PROCESS;
    3 : CALL WRITE_OUTPUT
END
```

### 6.5 REPEAT-UNTIL

```
REPEAT
    statements
UNTIL condition
```

A post-test loop: the body (which may contain multiple semicolon-separated statements) executes at least once, then repeats until `condition` is non-zero.

```
(* Read digits until a non-digit is found *)
REPEAT
    CH := GETCHAR;
    DIGIT := CH - 48
UNTIL CH < 48

### 6.6 DO

```
DO ;
DO statement
DO statement UNTIL break_condition
DO ;         UNTIL break_condition
```

`DO` is a general loop form. The body is executed repeatedly until the optional `UNTIL` condition becomes non-zero. When no `UNTIL` clause is present, the loop runs forever (useful for spin-waits and event loops). `DO ;` is the minimal form: an empty body with no exit condition — an infinite loop.

The `UNTIL` check occurs **after** each body execution (post-test), so for `DO stmt UNTIL cond`, the body always runs at least once.

Control flow:
```
→ do.body:  [body, may be empty]
→ do.until: if break_cond → do.exit    ← only when UNTIL present
→           else          → do.body
→ do.exit:
```
Without `UNTIL`, the back-edge is unconditional and no exit block exists.

Examples:

```
(* Infinite spin-wait — smallest possible loop *)
DO ;

(* Poll until a hardware flag is set *)
DO ;
UNTIL STATUS^ AND '0000000010000000'B

(* Read-process loop: body runs at least once *)
DO
    BEGIN
        CALL READ_RECORD;
        CALL PROCESS_RECORD
    END
UNTIL END_OF_FILE = 1

(* Bounded retry with UNTIL — exit when done or limit reached *)
DO
    BEGIN
        CALL TRY_OPERATION;
        RETRIES := RETRIES + 1
    END
UNTIL DONE = 1
```

### 6.7 GOTO and Labels

PL-11 supports labeled statements and `GOTO` for low-level control flow, primarily for implementing machine-level constructs:

```
    GOTO LABEL1;
    ...
LABEL1:
    statement
```

`GOTO` should be used sparingly. It cannot jump into a block from outside.

---

## 7. Procedures

### 7.1 Declaration

Procedures are declared within a block's declaration section:

```
PROCEDURE name (parameter_list) ;
BEGIN
    declarations
    statements
END ;
```

A procedure with no parameters:

```
PROCEDURE INIT;
BEGIN
    COUNT := 0;
    SUM   := 0
END;
```

### 7.2 Parameters

PL-11 supports three parameter passing modes, using ALGOL-68 / Ada-style mode annotations:

| Mode     | Keyword  | Meaning                                         |
|----------|----------|-------------------------------------------------|
| Value    | `IN`     | Caller's value is copied; callee cannot modify caller's variable |
| Result   | `OUT`    | Callee writes result back to caller's variable; initial value undefined |
| Value-Result | `IN OUT` | Caller's value is copied in; result is copied back on return |

```
PROCEDURE ADD (IN WORD A, IN WORD B, OUT WORD RESULT);
BEGIN
    RESULT := A + B
END;
```

```
PROCEDURE SWAP (IN OUT WORD X, IN OUT WORD Y);
BEGIN
    WORD TEMP;
    TEMP := X;
    X    := Y;
    Y    := TEMP
END;
```

If the mode keyword is omitted, `IN` (value) is assumed.

### 7.3 Calling Procedures

Procedures are invoked with `CALL`:

```
CALL INIT;
CALL ADD(3, 4, RESULT);
CALL SWAP(A, B);
```

### 7.4 Functions (Value-Returning Procedures)

A procedure can return a value by declaring a return type before its name:

```
WORD PROCEDURE MAX (IN WORD A, IN WORD B);
BEGIN
    IF A > B THEN RETURN(A) ELSE RETURN(B)
END;
```

Functions are called as expressions (without `CALL`):

```
Z := MAX(X, Y) + 1;
```

### 7.5 Recursion

Procedures may call themselves recursively:

```
WORD PROCEDURE FACTORIAL (IN WORD N);
BEGIN
    IF N <= 1 THEN RETURN(1)
    ELSE RETURN(N * FACTORIAL(N - 1))
END;
```

PL-11 allocates procedure local variables on the stack, so recursion is supported naturally.

---

## 8. Hardware and Machine Access

PL-11's distinguishing feature is direct access to PDP-11 hardware.

### 8.1 Registers

The PDP-11 has 16 registers named R0 through R15 (with R13=SP, R14=LR or FP, R15=PC on some configurations; conventionally R6=SP, R7=PC for the PDP-11).

PL-11 allows referencing registers by name in expressions and assignments:

```
R0 := 42;          (* load 42 into register R0 *)
X  := R1 + R2;     (* use register values in expressions *)
R3 := @BUFFER;     (* load address of BUFFER into R3 *)
```

Register variables are not allocated in memory; they refer directly to hardware registers. Using registers in high-level code is architecture-specific and produces a warning if the compiler targets a non-PDP-11 backend.

### 8.2 Direct Memory Addressing

A numeric literal followed by `B` (for "address") denotes an absolute memory address:

```
WORD DEVSTATUS;
DEVSTATUS := 177560B;        (* assign address 177560 octal to DEVSTATUS *)
DEVSTATUS^ := 0;             (* write 0 to memory location 177560 octal *)
```

This is the primary mechanism for memory-mapped I/O on the PDP-11.

### 8.3 Word Size and Alignment

PL-11 types map directly to PDP-11 storage units:

| PL-11 Type   | PDP-11 Storage | Bytes |
|--------------|----------------|-------|
| `BYTE`       | Byte           | 1     |
| `WORD`       | Word           | 2     |
| `LONG`       | Long word      | 4     |
| `REAL*4`     | F-format float | 4     |
| `REAL*8`     | D-format float | 8     |

The compiler ensures natural alignment (words on even addresses, etc.).

### 8.4 Inline Assembly

For operations not expressible in PL-11, inline PDP-11 assembly is supported using `ASM`:

```
ASM('MOV R0, R1');              (* single instruction *)
ASM('CLR @#177560');            (* clear device register *)
```

Inline assembly bypasses all type checking. Variables can be referenced by name within inline assembly; the compiler ensures they are accessible.

### 8.5 Stack Operations — PUSH and POP

PL-11 provides two built-in statements for direct stack manipulation, corresponding to the PDP-11 auto-decrement and auto-increment addressing modes.

```
PUSH expression       (* pre-decrement SP, store value at new SP  *)
POP  variable         (* load value from SP, post-increment SP     *)
```

**`PUSH expr`** evaluates `expr`, decrements SP by 2, then stores the value at the address now in SP:

```
PUSH A;              (* SP := SP - 2;  mem[SP] := A *)
PUSH R0;             (* push register R0 onto the stack *)
PUSH A + B * 2;      (* push any expression *)
```

**`POP target`** loads the 16-bit word at SP into `target`, then increments SP by 2:

```
POP A;               (* A := mem[SP];  SP := SP + 2 *)
POP R1;              (* pop into register R1 *)
```

PUSH and POP are LIFO — the last value pushed is the first value popped:

```
BEGIN
    WORD X, Y, TMP;
    X := 10;
    Y := 20;

    (* Swap X and Y via the stack *)
    PUSH X;
    PUSH Y;
    POP X;
    POP Y;

    (* Save and restore a register across a call *)
    PUSH R0;
    CALL COMPUTE;
    POP R0
END
```

SP corresponds to hardware register R6 on the PDP-11. On a hosted LLVM target, the compiler uses a 256-word simulated stack and keeps R6 synchronised as the simulated stack-pointer address.

---

## 9. Scope Rules

### 9.1 Lexical (Static) Scoping

PL-11 uses **lexical scoping**: a name refers to the declaration in the innermost enclosing block that contains the declaration. This is the same rule as ALGOL-60 and Pascal.

```
BEGIN
    WORD X;        (* outer X *)
    X := 10;
    BEGIN
        WORD X;    (* inner X — shadows outer X *)
        X := 20;      (* modifies inner X *)
        (* outer X is still 10 here *)
    END;
    (* X is 10 here — inner X is gone *)
END
```

### 9.2 Procedure Scope

A procedure's name is visible in the block where it is declared (from the point of declaration to the end of the block). Procedures are not visible before their declaration in the same block.

Mutually recursive procedures are not directly supported. A `FORWARD` declaration can be used:

```
PROCEDURE FORWARD ODD;   (* declare ODD before its body *)

WORD PROCEDURE EVEN (IN WORD N);
BEGIN
    IF N = 0 THEN RETURN(1) ELSE RETURN(ODD(N-1))
END;

WORD PROCEDURE ODD (IN WORD N);
BEGIN
    IF N = 0 THEN RETURN(0) ELSE RETURN(EVEN(N-1))
END;
```

### 9.3 Global Variables

Variables declared in the outermost (program-level) block are **global** and visible throughout the entire program (except where shadowed by a more local declaration with the same name).

### 9.4 No Name Overloading

PL-11 does not allow overloading: two declarations in the same scope cannot have the same name. Different scopes may (and commonly do) use the same names independently.

---

## 10. Sample Programs

### 10.1 Sum of Array Elements

```
BEGIN
    CONSTANT WORD N = 10;
    WORD A(N), SUM, I;

    (* Initialize array *)
    FOR I := 1 TO N DO
        A(I) := I * I;

    (* Compute sum *)
    SUM := 0;
    FOR I := 1 TO N DO
        SUM := SUM + A(I)

    (* SUM now contains 1 + 4 + 9 + ... + 100 = 385 *)
END
```

### 10.2 Bubble Sort

```
BEGIN
    CONSTANT WORD N = 20;
    WORD A(N), I, J, TEMP;

    PROCEDURE SORT;
    BEGIN
        FOR I := 1 TO N-1 DO
            FOR J := 1 TO N-I DO
                IF A(J) > A(J+1) THEN
                    BEGIN
                        TEMP    := A(J);
                        A(J)    := A(J+1);
                        A(J+1)  := TEMP
                    END
    END;

    (* ... fill A with data ... *)
    CALL SORT
END
```

### 10.3 Recursive Fibonacci

```
BEGIN
    WORD PROCEDURE FIB (IN WORD N);
    BEGIN
        IF N <= 1 THEN RETURN(N)
        ELSE RETURN(FIB(N-1) + FIB(N-2))
    END;

    WORD RESULT;
    RESULT := FIB(10)   (* = 55 *)
END
```

### 10.4 GCD Using WHILE

```
BEGIN
    WORD PROCEDURE GCD (IN WORD A, IN WORD B);
    BEGIN
        WORD R;
        WHILE B <> 0 DO
            BEGIN
                R := A MOD B;
                A := B;
                B := R
            END;
        RETURN(A)
    END;

    WORD X, Y, G;
    X := 48;
    Y := 18;
    G := GCD(X, Y)    (* G = 6 *)
END
```

### 10.5 Device Register Access (PDP-11 Specific)

```
BEGIN
    (* Read a character from the PDP-11 console terminal *)
    (* Console status register: 177560 octal             *)
    (* Console data register:   177562 octal             *)

    BIT*16 STATUS;
    BYTE CH;

    (* Wait for receiver ready (bit 7 of status) *)
    WHILE ((STATUS^ := 177560B^) AND '0000000010000000'B) = 0 DO;

    (* Read character from data register *)
    CH := 177562B^
END
```

### 10.6 Binary Search

```
BEGIN
    CONSTANT WORD N = 100;
    WORD A(N);

    WORD PROCEDURE BSEARCH (IN WORD KEY, IN WORD LO, IN WORD HI);
    BEGIN
        WORD MID, MIDVAL;
        IF LO > HI THEN RETURN(-1);
        MID    := (LO + HI) / 2;
        MIDVAL := A(MID);
        IF MIDVAL = KEY THEN RETURN(MID)
        ELSE IF MIDVAL < KEY THEN RETURN(BSEARCH(KEY, MID+1, HI))
        ELSE RETURN(BSEARCH(KEY, LO, MID-1))
    END;

    WORD POS;
    (* assume A is filled with sorted data *)
    POS := BSEARCH(42, 1, N)
END
```

---

## 11. Console Output — PRINT

PL-11 provides a built-in `PRINT` statement for formatted console output. It is modelled after C's `printf` and compiles directly to a `printf` call in the generated code.

### 11.1 Syntax

```
PRINT ( format_string )
PRINT ( format_string , expression { , expression } )
```

The first argument must be a quoted string literal. Zero or more additional arguments follow, one per format specifier in the string.

### 11.2 Format Specifiers

| Specifier | Argument type  | Output                 |
|-----------|----------------|------------------------|
| `%d`      | `BYTE`, `WORD` | Signed decimal integer |
| `%l`      | `LONG`         | Signed decimal long    |
| `%f`      | `REAL`         | Floating-point decimal |
| `%c`      | `BYTE`, `WORD` | Single character       |
| `%s`      | `CHARACTER`    | String                 |
| `%%`      | *(none)*       | Literal `%` character  |

Escape sequences follow C conventions: `\n` (newline), `\t` (tab), `\\` (backslash).

### 11.3 Examples

```
(* Simple message *)
PRINT('Hello, world!\n');

(* Integer values *)
WORD X, Y;
X := 6;
Y := 7;
PRINT('X = %d, Y = %d, product = %d\n', X, Y, X * Y);

(* Mixed types *)
WORD COUNT;
REAL AVG;
COUNT := 42;
AVG   := 3.14;
PRINT('Count: %d  Average: %f\n', COUNT, AVG);

(* Characters *)
BYTE CH;
CH := 65;
PRINT('Character: %c\n', CH);

(* Long integers *)
LONG BIG;
BIG := 100000;
PRINT('Big number: %l\n', BIG);
```

### 11.4 Notes

- Width and precision modifiers (e.g. `%8d`, `%.2f`) are not currently supported.
- The number of format specifiers must exactly match the number of extra arguments; a mismatch is a compile-time error.
- Each argument type is checked against its specifier at compile time.
- `PRINT` is a statement, not an expression; it cannot appear inside another expression.

---

## Appendix A: Reserved Words

```
AND         ARRAY       ASM         BEGIN       BIT
BYTE        CALL        CASE        CHARACTER   COMMENT
CONSTANT    DO          DOWNTO      ELSE        END
FLOAT       FOR         FORWARD     GOTO        IF
IN          INTEGER     LONG        MOD         NOT
OF          OR          OUT         PC          PRINT
PROCEDURE   REAL        REPEAT      RETURN      SHL
SHR         SHRA        SP          THEN        TO
UNTIL       WHILE       WORD        XOR
```

`INTEGER` and `FLOAT` are reserved aliases for `WORD` and `REAL` respectively.
`SP` and `PC` are reserved aliases for the stack-pointer and program-counter registers.
`COMMENT` is a reserved word that introduces a keyword comment (see §2.3); it is not available as an identifier.

## Appendix B: PDP-11 Register Names

The following identifiers are predefined as hardware register references when targeting PDP-11:

```
R0  R1  R2  R3  R4  R5  R6  R7
R8  R9  R10 R11 R12 R13 R14 R15
SP  PC
```

`SP` is an alias for `R6` (stack pointer); `PC` is an alias for `R7` (program counter).

## Appendix C: Operator Summary

| Operator  | Type       | Precedence | Associates |
|-----------|------------|------------|------------|
| `-` (unary) | Arithmetic | 1        | Right      |
| `NOT`     | Logical    | 1          | Right      |
| `*`       | Arithmetic | 2          | Left       |
| `/`       | Arithmetic | 2          | Left       |
| `MOD`     | Arithmetic | 2          | Left       |
| `AND`     | Logical    | 2          | Left       |
| `SHL`     | Bitwise    | 2          | Left       |
| `SHR`     | Bitwise    | 2          | Left       |
| `SHRA`    | Bitwise    | 2          | Left       |
| `+`       | Arithmetic | 3          | Left       |
| `-`       | Arithmetic | 3          | Left       |
| `OR`      | Logical    | 3          | Left       |
| `XOR`     | Logical    | 3          | Left       |
| `=`       | Relational | 4          | Left       |
| `/=`      | Relational | 4          | Left       |
| `<>`      | Relational | 4          | Left       |
| `<`       | Relational | 4          | Left       |
| `>`       | Relational | 4          | Left       |
| `<=`      | Relational | 4          | Left       |
| `>=`      | Relational | 4          | Left       |
| `:=`      | Assignment | 5 (stmt)   | Right      |
