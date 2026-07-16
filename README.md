<div align="center">

# Ash

<img src="logo.png" width="180" alt="Ash logo" />

</div>

---

Ash is a small programming language built from scratch in C — lexer, parser,
tree-walk interpreter, and a separate bytecode VM, all hand-written with no
external dependencies. It has closures, hash maps, first-class functions,
`try`/`catch`, and a colorized compiler that points straight at your mistakes.

```ash
fn make_adder(x) {
    return fn(y) { return x + y; };
}

let add5 = make_adder(5);
print add5(3); // 8

let nums = [1, 2, 3, 4, 5];
let evens = filter(nums, fn(n) { return n % 2 == 0; });
print evens; // [2, 4]

try {
    let m = {"name": "Ash"};
    print m["missing"];
} catch (e) {
    print "caught: " + e;
}
```

## Two engines, one language

Ash ships with two interchangeable interpreters that compile from the same source:

| | `ashc` | `ashvm` |
|---|---|---|
| Architecture | Tree-walk interpreter | Compiles to real bytecode, runs on a stack-based VM |
| Speed | Baseline | Computed-goto dispatch + hand-fused superinstructions |
| Feature set | Full — everything below | Full — full parity with `ashc`, including closures and `try`/`catch` |

Both support the entire language. `ashvm` exists because it's dramatically
faster; `ashc`'s simpler execution model made it the easiest place to build
and test new features first, before porting them over.

## Performance

Benchmarked against equivalent programs in C++ (`g++ -O2`) on the same machine:

| Benchmark | C++ | `ashvm` | `ashc` |
|---|---|---|---|
| `fib(30)` (recursive) | 0.005s | 0.06s (~12x slower) | 1.6s (~320x slower) |
| Sum of 0..20,000,000 | 0.004s | 0.12s (~30x slower) | 9.3s (~2,300x slower) |

`ashvm`'s speed comes from three things layered on top of each other: compiling
to real bytecode instead of re-parsing source on every loop iteration,
computed-goto instruction dispatch instead of a `switch` statement, and a
handful of hand-fused "superinstructions" for common patterns like
`x = x + y` and `while (i < n)` that collapse several bytecode ops into one.
A hand-written x86-64 JIT experiment (`benchmarks/native_jit.c`) that skips
bytecode entirely gets within ~2x of raw C++, which is roughly the ceiling
for anything short of a full optimizing native compiler.

## Language features

- Numbers, strings (with escape sequences), arrays, hash maps
- Functions, recursion, closures with by-value capture
- First-class functions — pass them around, store them in variables, call them indirectly
- `map`, `filter`, `reduce`, and 20+ other built-ins (string ops, file I/O, math)
- `try` / `catch` error handling
- `for (x in array)` and `while` loops
- Colorized, Rust-style compiler diagnostics: file:line:column, a source snippet, a caret, and a contextual hint
- Interactive REPLs for both engines with persistent variables across lines

## Getting started

```bash
make            # builds both ./ashc and ./ashvm

./ashc script.ash    # run a script on the full-featured tree-walk interpreter
./ashvm script.ash   # run a script on the fast bytecode VM

./ashc               # start the ashc REPL
./ashvm              # start the ashvm REPL
```

## Project structure
src/
lexer/        tokenizer, shared by both engines
value/        tagged Value type, hash map (ashc)
env/          variable scoping (ashc)
interpreter/  parser, statements, expressions, functions, closures,
error handling (ashc)
builtins/     len, map, filter, reduce, string/file ops (ashc)
vm/           bytecode compiler, stack VM, value type, hash map,
builtins (ashvm)
benchmarks/     .ash and .cpp benchmark programs, plus the x86-64 JIT experiment
tests/          example programs covering each language feature

## Editor support

A VS Code extension with syntax highlighting will come soon. 
We could build an icon theme too, but we'd rather not
it's too much work to maintain, so we'll leave that to other people. 
In the future, though, we plan to release a kind of IDE/code editor built with Ash,
for building things in Ash.