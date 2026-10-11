<div align="center">
  <h1>clox</h1>
  <p><strong>A bytecode virtual machine and compiler for the Lox language.</strong></p>

  <p>
    <img src="https://github.com/nikhilmishra1710/clox/actions/workflows/testing.yaml/badge.svg" alt="Build Status">
    <img src="https://img.shields.io/badge/language-C-blue.svg" alt="Language">
    <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License">
  </p>
</div>

---

## Introduction

`clox` is a compiler-interpreter hybrid that transforms Lox source code into a custom bytecode format, which is then executed by a specialized stack-based virtual machine. It is designed for efficiency and portability, focusing on the internals of language implementation, from lexical analysis to garbage collection.

## Key Features

### The Compiler
- **Recursive Descent**: A hand-written compiler that translates Lox tokens into optimized bytecode.
- **Bytecode Generation**: Efficient encoding of instructions into "chunks" for the VM to consume.
- **Static Analysis**: Handles variable scoping, closures, and function definitions.

### The Virtual Machine
- **Stack-Based Architecture**: Fast execution using a value stack for operands.
- **Opcode Optimization**: A lean instruction set designed to minimize dispatch overhead.
- **REPL Mode**: An interactive read-eval-print loop for real-time language exploration.

### The Runtime
- **Memory Management**: A robust **Mark-and-Sweep Garbage Collector** to automate memory reclamation.
- **Tagged Unions**: Efficient representation of Lox values (Numbers, Booleans, Strings, Native Functions).
- **Native Bridge**: A powerful API to define and call C functions directly from Lox scripts.

---

## Architecture

`clox` follows a linear pipeline to execute code:

**`Source Code`** $\rightarrow$ **`Scanner`** $\rightarrow$ **`Compiler`** $\rightarrow$ **`Bytecode (Chunks)`** $\rightarrow$ **`VM`** $\rightarrow$ **`Output`**

1. **Scanner**: Lexes the raw text into a stream of tokens.
2. **Compiler**: Parses tokens and emits bytecode instructions.
3. **VM**: Fetches instructions and manipulates the stack to produce results.

---

## Getting Started

### Prerequisites
Ensure you have the following tools installed:
- `gcc` or `clang` (C99 compatible compiler)
- `make`
- `lcov` & `genhtml` (optional, for coverage reports)

### Build
Clone the repository and run `make`:
```bash
make
```
The resulting binary will be located at `./bin/clox`.

### Usage

**Interactive REPL**
```bash
./bin/clox
```

**Execute a Script**
```bash
./bin/clox path/to/script.clox
```

#### Example Lox Code
```javascript
fun fib(n) {
  if (n <= 1) return n;
  return fib(n - 1) + fib(n - 2);
}

print "Fibonacci(10): " + fib(10);
```

---

## Development

### Common Commands
| Command | Description |
| :--- | :--- |
| `make` | Compiles the project |
| `make clean` | Removes build artifacts |
| `make test` | Runs the test suite via `tester.sh` |
| `make lint` | Checks code style via `.clang-format` |
| `make coverage` | Generates a coverage report in `coverage-report/` |

### Project Structure
| Directory/File | Purpose |
| :--- | :--- |
| `src/` | Core source code |
| `src/include/` | Header files and type definitions |
| `test/` | Test scripts and validation cases |
| `bin/` | Final compiled binaries |
| `obj/` | Intermediate object files |
| `Makefile` | Build automation rules |

---

## License
This project is open-source and available under the [MIT License](LICENSE).
