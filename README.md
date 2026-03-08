# PL-11 Compiler

A compiler front-end for **PL-11**, a systems programming language designed for the DEC PDP-11,
described in CERN Technical Report [CERN-74-24](https://cds.cern.ch/record/880468/files/CERN-74-24.pdf)
(R.D. Russell, 1974). The compiler emits LLVM IR, which can be lowered to native code by the
standard LLVM toolchain.

---

## Background

PL-11 was designed at CERN for writing real-time control and data-acquisition software on the
PDP-11 minicomputer. It sits in the same design space as PL/360 and early ALGOL-derived systems
languages: structured control flow, direct hardware access (register names, memory addresses,
inline assembly), and a simple static type system.

This implementation is an educational reconstructions from the original report. The target has
been shifted from PDP-11 machine code to LLVM IR, making the output portable and inspectable
while preserving the source language as closely as practical. Features that are inherently
PDP-11-specific (hardware register R0–R15 as first-class l-values, inline assembly, octal
address literals) are accepted and compiled, with a warning where the semantics are necessarily
approximated on a modern target.

---

## Language overview

PL-11 is an ALGOL-family, block-structured language with the following features:

- `BEGIN … END` blocks with lexically scoped declarations
- Types: `BYTE` (8-bit), `WORD`/`INTEGER` (16-bit), `LONG` (32-bit), `REAL`/`FLOAT` (32-bit
  float), `CHARACTER*N` (byte strings), `BIT*N` (bit strings)
- Arrays declared inline with dimension lists: `WORD A(10)`, `WORD B(3, 4)`
- Control flow: `IF-THEN-ELSE`, `WHILE-DO`, `FOR-TO/DOWNTO-DO`, `REPEAT-UNTIL`, `CASE-OF-END`;
  UNH extensions add `DO` loop and optional `UNTIL` clause on `WHILE` and `FOR`
- Procedures with typed parameters and `IN`/`OUT`/`IN OUT` passing modes; typed functions via
  `type PROCEDURE name(…)`
- `GOTO` with forward and backward labels
- Hardware access: register names (`R0`–`R15`, `SP`, `PC`), octal address literals, `ASM(…)`
  for inline assembly snippets
- Built-in `PRINT(fmt, …)` for formatted console output (maps to C `printf`)

---

## Extensions beyond CERN-74-24

The following features are present in this implementation but were not part of the original
report, or extend it in ways consistent with the spirit of the language:

### UNH extensions — Dr. Robert Russell, University of New Hampshire

The three loop-control extensions below were designed by Dr. Robert Russell at the University
of New Hampshire as part of ongoing work with the PL-11 language after the original CERN report.
They follow the same syntactic and semantic conventions as the base language and introduce no
new reserved words.

#### `UNTIL` clause on `WHILE` and `FOR` loops

Both loop forms accept an optional post-body early-exit test:

```
WHILE condition DO statement UNTIL break_condition
FOR var := start TO end DO statement UNTIL break_condition
```

`break_condition` is evaluated **after** the body completes each iteration. If it is non-zero,
the loop exits immediately — equivalent to an inline `IF break_condition THEN BREAK`. For `FOR`,
the check occurs before the increment step, so the loop variable retains its current value at
the exit point.

```
(* Sum integers until the running total exceeds 100 *)
WHILE I <= N DO
    BEGIN
        SUM := SUM + A(I);
        I := I + 1
    END
UNTIL SUM > 100

(* Linear search — stop at first match *)
FOR I := 1 TO N DO
    LAST := A(I)
UNTIL A(I) = KEY
```

The `UNTIL` keyword already existed in the original language (used by `REPEAT … UNTIL`); no
new reserved words were introduced.

#### `DO` loop

A general loop form with an optional body and optional `UNTIL` exit condition:

```
DO ;                          (* infinite loop, empty body — smallest valid loop *)
DO statement                  (* infinite loop with body *)
DO statement UNTIL condition  (* post-test loop — body always runs at least once *)
DO ;         UNTIL condition  (* condition-only post-test *)
```

When no `UNTIL` clause is present the loop runs forever, which is the intended behaviour for
spin-waits and event loops in systems software. When `UNTIL` is present the condition is tested
after each body execution, so the body always executes at least once — making `DO … UNTIL`
strictly post-test, in contrast to `WHILE … DO` which is pre-test.

```
(* Poll a hardware status register *)
DO ;
UNTIL STATUS^ AND '0000000010000000'B

(* Read-process loop — body runs at least once *)
DO
    BEGIN
        CALL READ_RECORD;
        CALL PROCESS_RECORD
    END
UNTIL END_OF_FILE = 1
```

### `PRINT` statement

A built-in formatted output statement mapping directly to C `printf`. Not in the original
report (which targeted bare-metal PDP-11 with no OS I/O library), but essential for any
practical testing on a hosted system.

```
PRINT('Result: %d, Pi ~ %f\n', N, PI)
```

Supported specifiers: `%d` (WORD/BYTE), `%l` (LONG), `%f` (REAL), `%c` (character code),
`%s` (CHARACTER string), `%%` (literal `%`).

### LLVM IR back-end

The original compiler targeted PDP-11 machine code directly. This implementation replaces the
code-generation back-end with LLVM IR via `llvm::IRBuilder<>`, enabling:

- Optimisation passes (`opt`)
- Native code generation for any LLVM-supported target (x86-64, AArch64, …)
- IR inspection for teaching purposes (`--emit-llvm`)

### `INTEGER` as a type alias for `WORD`

The original report uses `INTEGER` only in informal descriptions; the implemented type keywords
are `WORD`, `BYTE`, `LONG`, `REAL`, `CHARACTER`, `BIT`. This implementation accepts `INTEGER`
as a synonym for `WORD` for readability.

---

## Repository layout

```
pl11/
├── CMakeLists.txt          Build system (links against LLVM via llvm-config)
├── pl11build               Shell script: .pl11 → native executable
├── docs/
│   └── pl11-language-reference.md   Full language reference
├── grammar/
│   ├── pl11.ebnf           Formal EBNF grammar
│   └── pl11.g4             ANTLR4 grammar
├── src/
│   ├── lexer/              Hand-written lexer
│   ├── parser/             Recursive-descent parser
│   ├── ast/                AST node definitions (pl11_ast.h)
│   ├── sema/               Type checker and symbol table
│   ├── codegen/            LLVM IR emitter
│   └── main.cpp            Driver
└── tests/
    ├── hello.pl11
    ├── fibonacci.pl11
    ├── gcd.pl11
    ├── control_flow.pl11
    ├── sort.pl11
    ├── print_demo.pl11
    ├── while_until.pl11    WHILE … UNTIL extension test (UNH)
    ├── for_until.pl11      FOR  … UNTIL extension test (UNH)
    ├── do_loop.pl11        DO loop extension test (UNH)
    └── primes.pl11         First 1000 primes via trial division (system verification)
```

---

## Building

### Prerequisites

- CMake ≥ 3.16
- C++17 compiler (GCC or Clang)
- LLVM 17, 18, or 19 (with `llvm-config`, `llc`, and `clang` on `PATH`)

On Debian/Ubuntu:

```sh
apt install llvm-19 clang-19 cmake build-essential
```

### With LLVM back-end (recommended)

```sh
cmake -S . -B build-llvm -DPL11_WITH_LLVM=ON
cmake --build build-llvm --parallel
```

> **Raspberry Pi / Debian note:** if `libzstd-dev` is not installed (only `libzstd1`), the
> linker will fail to find `libzstd.so`. Create a symlink manually before building:
> ```sh
> mkdir -p build-llvm/lib
> ln -sf /usr/lib/aarch64-linux-gnu/libzstd.so.1 build-llvm/lib/libzstd.so
> ```
> The CMakeLists.txt adds `-Lbuild-llvm/lib` automatically once the directory exists.

### Without LLVM (lexer/parser/sema only)

```sh
cmake -S . -B build -DPL11_WITH_LLVM=OFF
cmake --build build --parallel
```

---

## Usage

```sh
# Lex tokens only
./build-llvm/pl11c --lex   program.pl11

# Parse and print AST summary
./build-llvm/pl11c --parse program.pl11

# Semantic analysis only
./build-llvm/pl11c --sema  program.pl11

# Emit LLVM IR
./build-llvm/pl11c --emit-llvm program.pl11

# Compile to native executable (uses pl11build wrapper)
./pl11build program.pl11 -o program
./program
```

The `pl11build` script handles the full pipeline: `pl11c --emit-llvm` → `llc` → `clang` link.
Pass `--keep-ir` to retain `.ll` files, or `-v` for verbose output showing each command.

---

## Running the tests

```sh
cd build-llvm
ctest --output-on-failure
```

---

## Compiler pipeline

```
PL-11 source
    │
    ▼
Lexer          src/lexer/pl11_lexer.cpp
    │              Hand-written; case-insensitive keywords;
    │              identifiers normalised to uppercase.
    ▼
Parser         src/parser/pl11_parser.cpp
    │              Recursive descent LL(1); one function per grammar rule.
    ▼
AST            src/ast/pl11_ast.h
    │              Typed node hierarchy; unique_ptr ownership.
    ▼
Sema           src/sema/pl11_sema.cpp
    │              Scope stack; type inference; procedure/function checking.
    ▼
Codegen        src/codegen/pl11_codegen.cpp
    │              llvm::IRBuilder<>; opaque pointers (LLVM 17+).
    ▼
LLVM IR  ──►  opt / llc / clang  ──►  native executable
```

---

## References

Russell, R.D. (1974). *PL-11: A Programming Language for the DEC PDP-11 Computer*. CERN
Technical Report CERN-74-24. European Organisation for Nuclear Research, Geneva.

Russell, R.D. University of New Hampshire. Extensions to PL-11: `UNTIL` clause for `WHILE`
and `FOR` loops; `DO` loop construct.
