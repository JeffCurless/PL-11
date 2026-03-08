# Compiling PL-11 Programs

## Prerequisites

Build the compiler first. From the project root:

```sh
# Without LLVM (lex/parse/sema only)
cmake -S . -B build -DPL11_WITH_LLVM=OFF
cmake --build build

# With LLVM 19 (full IR emission)
ln -sf /usr/lib/aarch64-linux-gnu/libzstd.so.1 build-llvm/lib/libzstd.so
cmake -S . -B build-llvm -DPL11_WITH_LLVM=ON
cmake --build build-llvm
```

The compiler binary is `build/pl11c` or `build-llvm/pl11c`.

---

## Modes

### Check tokens (lexer output)

Useful for debugging or understanding how the source is tokenised.

```sh
./pl11c --lex hello.pl11
```

Output: one token per line with location, kind, and raw text.

---

### Check syntax (parser only)

Parses the program and reports whether the structure is valid.

```sh
./pl11c --parse hello.pl11
```

On success:
```
Parse successful. Program block has 3 declaration(s) and 4 statement(s).
```

---

### Check types (semantic analysis)

Runs type checking and scope resolution on top of parsing.

```sh
./pl11c --sema hello.pl11
```

On success:
```
Semantic analysis passed.
```

---

### Emit LLVM IR

Requires the LLVM build. Prints human-readable LLVM IR to stdout.

```sh
./pl11c --emit-llvm hello.pl11
```

Redirect to a file:

```sh
./pl11c --emit-llvm hello.pl11 > hello.ll
```

---

## End-to-End: Source to Executable

PL-11 has no built-in I/O, so a runnable binary requires a C wrapper that
calls into the generated IR. The steps below compile `gcd.pl11` and verify
the result.

### 1. Emit IR

```sh
./pl11c --emit-llvm tests/gcd.pl11 > gcd.ll
```

### 2. Verify the IR (optional)

```sh
opt-19 --passes="verify" gcd.ll -o /dev/null
```

### 3. Compile IR to an object file

```sh
llc-19 -filetype=obj gcd.ll -o gcd.o
```

### 4. Link with a C driver

Write a small C file that calls the PL-11 program's `main`:

```c
// driver.c
#include <stdio.h>
int pl11_main();           // forward declaration of the compiled PL-11 block

int main() {
    pl11_main();
    return 0;
}
```

> Note: the PL-11 program block compiles to a function named `main` in the IR.
> Rename it with `sed` or a post-processing step if you need to link it as a
> library rather than a standalone binary.

```sh
clang-19 driver.c gcd.o -o gcd
./gcd
```

### Shortcut: IR → binary in one step

```sh
./pl11c --emit-llvm tests/hello.pl11 | llc-19 -filetype=obj -o hello.o
clang-19 hello.o -o hello
./hello
```

---

## Inspect the IR with opt passes

```sh
# Pretty-print with annotations
./pl11c --emit-llvm tests/fibonacci.pl11 | opt-19 --passes="mem2reg,instcombine,print<domtree>" -S

# Run standard optimisations
./pl11c --emit-llvm tests/gcd.pl11 | opt-19 -O2 -S > gcd_opt.ll
```

---

## Compiler flags summary

| Flag            | Description                                  |
|-----------------|----------------------------------------------|
| `--lex`         | Print tokens and exit                        |
| `--parse`       | Parse only, print summary and exit           |
| `--sema`        | Parse + type check, exit                     |
| `--emit-llvm`   | Emit LLVM IR to stdout (requires LLVM build) |
| `-o <file>`     | Output file for object code                  |
| `-h`, `--help`  | Show help                                    |

---

## Common errors

**`Array dimension must be an integer constant`**
Array sizes must be numeric literals in the current implementation.
Use `WORD A(8)` rather than a named constant in the declaration.

**`'X' is not a function`**
A procedure was called without `CALL`, or a variable name was used as a
function in an expression context.  Use `CALL PROC(args)` for procedures;
use `RESULT := FUNC(args)` for functions (those declared with a return type).

**`Redeclaration of 'X'`**
Two declarations in the same block share a name.  Move one into a nested
`BEGIN`-`END` block or rename it.
