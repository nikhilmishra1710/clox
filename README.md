# clox
[![Build Status](https://github.com/nikhilmishra1710/clox/actions/workflows/testing.yaml/badge.svg)](https://github.com/nikhilmishra1710/clox/actions)

`clox` is a bytecode virtual machine with a compiler — a compiler–interpreter hybrid inspired by Bob Nystrom's *Crafting Interpreters*.  
It compiles source code into bytecode and immediately executes it on a simple stack-based virtual machine.

---

## Features
- Bytecode compiler for a small language (based on Lox).
- Stack-based virtual machine for executing bytecode.
- REPL mode for interactive evaluation.
- Script execution from files.
- Written in C for simplicity and portability.

---

## Build

Clone the repository and build using `make`:

```
make
```

This produces the `clox` binary inside the `bin/` directory.

---

## Usage

Run in interactive REPL mode:
```
./bin/clox
```

Run a script file:
```
./bin/clox examples/hello.clox
```

---

## Development

### Clean build
```
make clean
```

### Run tests
```
make test
```

### Lint code
```
make lint
```

### Coverage (requires `lcov` & `genhtml`)
```
make coverage
```
Generates a coverage report at `coverage-report/index.html`.

---

## Project Structure
```
src/        # Source code
src/include # Header files
tests/      # Test scripts
bin/        # Compiled binaries
obj/        # Object files
lib/        # Static libraries
```

---

## License
This project is for educational and hobby purposes, based on *Crafting Interpreters* by Bob Nystrom.
