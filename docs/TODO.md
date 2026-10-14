Language features to implement

High impact / most natural next steps
1. Lists (arrays) — the biggest gap. var a = [1, 2, 3];, indexing a[0], a[0] = x, methods like push/pop/len. Needs OBJ_LIST, OP_BUILD_LIST, OP_INDEX_GET/SET. Your native-class machinery makes the methods easy.
2. Maps/dictionaries — you already have a hash table in C (table.c); exposing it as {"key": value} is mostly parser + a new object type.
3. String operations — s.length(), s[i], slicing, and string interpolation ("x = ${x}").
4. break / continue — no tokens for them yet. Doable with jump patching in the loop compilers, and would make switch more conventional too.
5. Compound assignment & inc/dec — +=, -=, *=, /=, ++, --. Pure compiler sugar.
6. % and bitwise operators — & | ^ << >> ~. One opcode + one parse-rule row each.

Medium effort, big expressiveness
7. Anonymous functions / lambdas — var f = fun (a, b) { return a + b; };
8. Exceptions — try/catch/throw with stack unwinding in vm.c.
9. Modules — import "foo.lox" with per-module globals.
10. Static methods & getters on classes.
11. Default & variadic parameters for user functions (your natives already support variadic via arity = -1).
12. Block comments /* ... */ — scanner only handles // right now.

Ambitious / fun
13. Iterators & for x in list loops, ranges (1..10).
14. A real standard library — math (sqrt, floor, sin…), string, os/env, time, built on your ObjNativeClass bridge.
15. Operator overloading via metamethods (__add, __index, Lua-style) for classes.
16. Integer type distinct from doubles, UTF-8-aware strings.

Improvements to the current implementation

Actual bugs / cleanups
- compiler.c:409 — leftover printf("parsed ternary expression\n") debug print fires on every ternary. Remove it.
- nativeFunction.c:93 — fileRead builds the string with length sz (from ftell) instead of nread (actual bytes read). If fread returns short, the string contains uninitialized bytes. Use nread.
- fileReadline keeps the trailing \n in the returned string while input() strips it — inconsistent; pick one.
- fileClose (nativeFunction.c:131-146) reaches through args[-1] to null the receiver's data — the comment itself admits it's a hack. Pass the receiver explicitly to native methods instead.
- input() silently truncates lines longer than 1024 chars — loop on fgets or use getline.

Robustness
- GC gray-stack: make sure it grows dynamically under deep object graphs (book uses realloc — verify yours handles overflow).
- Add GC stress tests (DEBUG_STRESS_GC builds) and error-path tests to test/ — you have feature tests, fewer failure-mode ones.
- Consider -Wall -Wextra -Wpedantic in the Makefile (with -Werror in CI) if not already there.

Performance
- Inline caching for OP_GET_PROPERTY / OP_INVOKE — biggest realistic win for method-heavy code.
- Superinstructions — fuse common pairs (OP_GET_LOCAL + OP_ADD, comparison + OP_JUMP_IF_FALSE).
- Computed-goto dispatch instead of the switch in the interpreter loop (GCC/Clang extension).
- Constant folding in the compiler (1 + 2 → OP_CONSTANT 3).

Developer experience
- REPL: readline history + echo expression results (print non-nil expression statements).
- Runtime errors that print the offending source line with a caret, not just line numbers + stack trace.
- A --dump-bytecode CLI flag instead of recompiling with DEBUG_PRINT_CODE.