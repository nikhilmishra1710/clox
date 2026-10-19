# clox Roadmap & Issue Tracker

## Section 1: Features

### High Impact / Core Next Steps

1. **Lists / Dynamic Arrays** (`var a = [1, 2, 3];`, indexing `a[0]`, `a[0] = x`, methods `push`/`pop`/`len` via `OBJ_LIST`, `OP_BUILD_LIST`, `OP_INDEX_GET/SET`)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/list/` folder

2. **Maps / Dictionaries** (Expose `table.c` as `{"key": value}` literals via `OBJ_MAP`, `OP_BUILD_MAP`, and `OP_INDEX_GET/SET`, plus `has`/`remove`/`keys`/`len` methods)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/map/` folder

3. **String Operations & Interpolation** (`s.length()`, `s[i]`, slicing `s[a:b]`, escape sequences `\n`/`\t`/`\"`, and `"x = ${x}"` string interpolation)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/string/` folder

4. **Loop & Switch Control Flow (`break` / `continue`)** (Jump patching in loop/switch compilers and `fallthrough`/break semantics in `switch`)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/for/`, `test/while/`, and `test/switch/` folders

5. **Compound Assignment & Increment/Decrement** (`+=`, `-=`, `*=`, `/=`, `++`, `--` as compiler sugar)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/assignment/` and `test/operator/` folders

6. **Modulo & Bitwise Operators** (`%`, `&`, `|`, `^`, `<<`, `>>`, `~` — one opcode and parse-rule row each)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/operator/` folder

### Medium Effort / High Expressiveness

7. **Anonymous Functions / Lambdas** (`var f = fun (a, b) { return a + b; };` expression-level function compilation)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/function/` and `test/closure/` folders

8. **Exceptions (`try` / `catch` / `throw`)** (Exception handler stack per `CallFrame` and stack/upvalue unwinding in `vm.c`)
   - [X] Implementation
   - [X] Basic sanity
   - [ ] Add test cases in `test/exception/` folder

9. **Module System (`import "foo.lox"`)** (Per-module isolated globals, module caching, and relative path resolution)
   - [ ] Implementation
   - [ ] Basic sanity
   - [ ] Add test cases in `test/module/` folder

10. **Static Class Methods & Getters** (`class Math { static square(x) { ... } }` and parameterless property getters)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/class/` and `test/method/` folders

11. **Default & Variadic Parameters for User Functions** (Extend user-defined `ObjFunction` to match native `arity = -1` variadics and default values)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/function/` folder

12. **Block Comments (`/* ... */`)** (Multi-line comment skipping in `scanner.c` with proper line tracking)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/comments/` folder

### Ambitious / Advanced Extensions

13. **Iterators, `for (var x in col)` Loops & Ranges (`1..10`)** (Iterator protocol for Lists, Maps, Strings, and Range objects)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/for/` folder

14. **Standard Library Suite** (`Math`, `Env`/`OS`, `Time`, `JSON` built on the `ObjNativeClass` bridge)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/stdlib/` folder

15. **Operator Overloading via Metamethods** (Lua/Python-style `__add`, `__sub`, `__index`, `__str` on user classes)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/class/` folder

16. **Distinct 64-bit Integer Type & UTF-8-Aware Strings** (Extend NaN-boxing/Value representation for exact integers and UTF-8 codepoint indexing)
    - [ ] Implementation
    - [ ] Basic sanity
    - [ ] Add test cases in `test/number/` and `test/string/` folders

### VM Performance & DX Enhancements

17. **Constant Folding in Compiler** (Fold constant literals like `1 + 2` -> `OP_CONSTANT 3` at compile time)
    - [ ] Implementation
    - [ ] Basic sanity

18. **Inline Caching for `OP_GET_PROPERTY` / `OP_INVOKE`** (Cache class pointer and method/field lookup at bytecode call sites)
    - [ ] Implementation
    - [ ] Basic sanity

19. **Computed-Goto Bytecode Dispatch & Superinstructions** (Replace `switch` in `run()` with GCC/Clang jump table and fuse hot opcode pairs)
    - [ ] Implementation
    - [ ] Basic sanity

20. **Enhanced CLI, REPL & Diagnostics** (`--dump-bytecode` flag, REPL `readline` history + auto-printing non-nil expressions, and source-line caret error reporting)
    - [ ] Implementation
    - [ ] Basic sanity

---

## Section 2: Bugs and Issues

- [ ] **`table.c:40-42` (`findEntry` Tombstone Bug):** Remove the duplicate `if (entry->key == key || entry->key == NULL) return entry;` check that prematurely returns a tombstone before probing the rest of the chain.
- [ ] **`table.c:141` (`tableSet` Count Bug):** Remove `if (!isNewKey) table->count++;` so overwriting an existing key does not artificially inflate `table->count`.
- [ ] **`memory.c:191` & `vm.c:551` (`vm.switchValue` GC Root & Nesting Bug):** `vm.switchValue` is never marked in `markRoots()`, risking premature GC sweep during `switch` evaluation; nested `switch` statements also overwrite `vm.switchValue`. Keep the switch value on the VM stack instead.
- [ ] **`compiler.c:409` (Leftover Debug Print):** Remove `printf("parsed ternary expression\n");` which fires on every ternary compilation and breaks test stdout comparison.
- [ ] **`nativeFunction.c:93` (`fileRead` Buffer Length Bug):** `makeString(buf, (int) sz)` uses `ftell` size `sz` instead of actual bytes read `nread`. Pass `(int) nread` to prevent uninitialized trailing bytes.
- [ ] **`nativeFunction.c:110` (`fileReadline` vs `inputNative` Newline Inconsistency):** `fileReadline` retains the trailing `\n` while `inputNative` strips it. Standardize behavior across both.
- [ ] **`nativeFunction.c:139` (`fileClose` Receiver Mutation Hack):** `fileClose` reaches into `args[-1]` to null out `AS_NATIVE_INSTANCE(args[-1])->data`. Pass `ObjNativeInstance*` (or receiver `Value`) explicitly to `NativeMethodFn`.
- [ ] **`nativeFunction.c:30` (`inputNative` Line Truncation):** `fgets(buffer, 1024, stdin)` silently truncates inputs longer than 1023 characters. Use dynamic buffer growth or `getline()`.
- [ ] **`chunk.h:9` & `compiler.c:167` (`OP_CONSTANT_LONG` Incomplete):** `OP_CONSTANT_LONG` is in `chunk.h` and `debug.c`, but `makeConstant()` still caps constants at 255 and `vm.c` lacks an `OP_CONSTANT_LONG` handler.
- [ ] **`main.c:58` (`runFile` Memory Leak):** `char* source = readFile(filePath);` is never freed after `interpret(source)` finishes.
- [ ] **`test/` Suite Coverage Gaps:** Add dedicated test suites for custom extensions (`ternary ?:`, `switch/case/default`, and `File` native class) plus GC stress tests (`DEBUG_STRESS_GC`) and failure-mode tests.

---

## Section 3: Docs That Need to Be Completed and More Reading Material

- [ ] **`docs/architecture.md` (VM & Compiler Internals)** 1. High-level pipeline diagram: `Source -> Scanner -> Pratt Parser / Single-Pass Compiler -> Bytecode Chunk -> VM Stack & Frame Loop`. 2. Value representation: Explain how `NAN_BOXING` packs `double`, `nil`, `bool`, and 48-bit `Obj*` pointers into a single 64-bit `uint64_t`. 3. Memory management & GC: Tri-color mark-and-sweep lifecycle (`white -> grayStack -> black`), string interning (`vm.strings`), and when `collectGarbage()` triggers. 4. Closures & Upvalues: How `ObjUpvalue` transitions stack variables from open (stack-pointing) to closed (heap-hoisted) via `OP_CLOSE_UPVALUE`. 5. Native C Bridge: How `ObjNative` functions and `ObjNativeClass` / `ObjNativeInstance` (`fileClass`, `listMethods`, `mapMethods`) interface with the VM stack and GC finalizers.
- [ ] **`docs/bytecode_spec.md` (Bytecode Instruction Reference)** 1. Document every `OpCode` in `chunk.h`, its byte length, operands (1-byte index, 2-byte jump offset, constant pool index), and exact stack effect (e.g., `OP_INDEX_SET`: `[list, index, val] -> [val]`).
- [ ] **`docs/language_reference.md` (Lox+ Language & Stdlib Guide)** 1. Syntax reference covering standard Lox plus your extensions: ternary `?:`, `switch/case/default`, Lists `[...]`, Maps `{...}`, string interpolation `"${...}"`, and built-in `File` / `clock` / `input` APIs.
- [ ] **`docs/testing_and_coverage.md` (Testing & Benchmarking Guide)** 1. Explain how `tester.sh` works, how `.yaml` specs map `.lox` inputs to `.out.expected` and `.err.expected`, and how to generate `lcov`/`gcov` HTML coverage reports.
- [ ] **[Crafting Interpreters — Part III: A Bytecode Virtual Machine (Robert Nystrom)](https://craftinginterpreters.com/a-bytecode-virtual-machine.html)** — Core reference for `clox`'s Pratt parser, NaN boxing, upvalues, and mark-and-sweep GC.
- [ ] **[ Pratt Parsing: "Top Down Operator Precedence" (Vaughan Pratt) / Bob Nystrom's "Pratt Parsers: Expression Parsing Made Easy"](https://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/)** — Deep dive into prefix/infix precedence climbing tables (`ParseRule rules[]`).
- [ ] **[Lua 5.0 / Wren VM Implementation Papers](https://www.lua.org/doc/jucs05.pdf)** — Reading material for implementing `ObjList`, `ObjMap` hash tables, metamethods, and upvalue optimizations in a C VM.
- [ ] **[Inline Caching & Superinstructions in Bytecode VMs ("Efficient Interpretation using Quickening")](https://www.complang.tuwien.ac.at/forth/threaded-code.html)** — Reading material for computed-goto dispatch (`&&op_label`) and polymorphic inline caches (PICs).
