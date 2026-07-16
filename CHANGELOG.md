## 2026-07-16 04:57
restructured into component-based file tree (lexer/value/env/interpreter/builtins); added arrays, indexing, index-assignment, for-in loops, logical operators (&& || !), modulo, and builtins (len, push, input, str, num, sqrt, abs, floor)

## 2026-07-16 05:05
added bytecode compiler + stack-based VM (src/vm/) as ashvm binary alongside the tree-walk ashc interpreter; numeric-only for now (no strings/arrays in VM path yet)

## 2026-07-16 05:10
added native_jit.c: hand-assembled x86-64 machine code JIT (mmap+PROT_EXEC) for one fixed loop pattern, proving native codegen closes the gap to C++; not a general Ash compiler yet

## 2026-07-16 05:14
ashvm: switched stack/constants from double to int64_t and replaced switch dispatch with computed-goto threading — measuring real impact on fib(30) and loop benchmarks

## 2026-07-16 05:19
ashvm: added peephole-fused superinstructions (OP_INC_LOCAL, OP_ADD_LOCAL_LOCAL, OP_LOOP_LT_JUMP) mirroring GCC's instruction-combining passes; targets the loop accumulate/compare/branch pattern specifically

## 2026-07-16 05:24
generalized VM peephole fusion: OP_ACC_LOCAL now handles +,-,* with local-or-constant operand; OP_CMP_JUMP now handles all 6 comparisons with local-or-constant operand, applied to BOTH if and while (not just while); verified against multiple distinct benchmark shapes, not just the original loop

## 2026-07-16 05:29
ashc: added first-class functions (functions as values, function-typed variables, indirect calls), lambda literals (fn(params){body} as an expression), and builtins map/filter/reduce; reorganized root directory (benchmarks/ for all bench files, tests/ for example .ash programs, removed stray test.ash)

## 2026-07-16 05:33
fixed critical bug: map/filter/reduce's nested function calls were clobbering the outer parser's token position without restoring it (call_by_index now saves/restores caller position at the single shared call site)

## 2026-07-16 05:39
ashc: added real closures (lambdas capture enclosing scope by value at creation time), new Closure type in src/interpreter/closures.c, map/filter/reduce accept both plain functions and closures

## 2026-07-16 05:42
ashc: added hash maps (VAL_MAP), map literals {"key": val}, get/set via existing bracket syntax, builtins keys/values/has/delete, extended len() to maps; new src/value/hashmap.c component

## 2026-07-16 05:45
ashc: added file I/O builtins (read_file, write_file, append_file, file_exists) and string builtins (split, join, substring, indexOf, replace, upper, lower, trim)

## 2026-07-16 05:47
fixed bug: string literals never processed escape sequences (\n, \t, \", \\ were passed through literally). Added copy_string_escaped() in value.c, used for string literals and map keys.

## 2026-07-16 05:49
fixed lexer bug: string scanning didn't skip escaped quotes (\"), so strings containing an escaped quote were truncated early. Lexer now skips over any backslash-escaped character while scanning for the closing quote.

## 2026-07-16 05:52
benchmarked ashc hashmap performance vs C++ std::unordered_map after rounds 1-3 (closures/hashmaps/string+file builtins) - confirms ashc has had no speed optimization work, unlike ashvm

## 2026-07-16 07:42
ashc: added try/catch error handling via setjmp/longjmp, with explicit scope-depth and parser-position repair after a caught error (new src/interpreter/error.c); converted genuine runtime errors (undefined var, type errors, out-of-bounds, missing map keys, arg mismatches, file I/O failures) to be catchable, while parse/syntax errors remain hard crashes

## 2026-07-16 07:52
cleaned up trailing embedded newlines in auto-converted runtime_error() messages (leftover from their original fprintf format strings), fixing double-blank-line output when caught errors are printed

## 2026-07-16 07:58
ashc: colorized Rust-style error output (file:line:col, source snippet, caret pointer, contextual help hints); fixed line/column accuracy by computing position via a scan of the original untouched source buffer instead of the interpreter's resettable internal line counter

## 2026-07-16 08:02
fixed off-by-one bug in error hint lookup (hardcoded prefix lengths were wrong, e.g. 19 instead of 18 for 'undefined variable', silently failing every strncmp); now computes prefix length via strlen() automatically instead of manual counting

## 2026-07-16 08:05
ashvm round 1 of feature-merge: added tagged VMValue type (numbers + strings) replacing raw int64_t stack values; string literals, concatenation via +, string equality via ==/!=, all opcodes and fused superinstructions updated to be type-aware

## 2026-07-16 08:10
ashvm round 2 of feature-merge: added arrays (VM_ARRAY type, array literals, indexing read/write) with new opcodes OP_ARRAY/OP_INDEX_GET/OP_INDEX_SET; full-file rewrites this round to avoid the silent-corruption pattern seen with partial regex edits earlier

## 2026-07-16 08:18
ashvm: ported hash maps, function values, indirect calls, map/filter/reduce (via named functions), and all 23 ashc builtins into the VM; new synchronous nested-call mechanism (vm_call_function_sync) enables higher-order builtins; true inline lambda closures still pending as a final round

## 2026-07-16 08:27
ashvm: added real closures (fn(x){...} lambda literals capturing enclosing scope by value, via reserved local slots placed after params + OP_MAKE_CLOSURE), map/filter/reduce now accept closures as well as named functions

## 2026-07-16 08:28
fixed cosmetic strncpy-truncation warnings in the lambda-naming code by clamping namelen to the buffer size

## 2026-07-16 08:29
fixed persistent strncpy-truncation warnings by switching to memcpy+manual-null-terminate (GCC's -Wstringop-truncation is overly conservative for variable-length strncpy even when provably safe)

## 2026-07-16 08:34
ashvm: added try/catch via setjmp/longjmp with compile-time-known catch-block bytecode offsets (OP_TRY_PUSH/OP_TRY_POP), converted all vm.c and builtins.c runtime error sites to be catchable; VM merge project now feature-complete relative to ashc

## 2026-07-16 08:35
fixed real setjmp/longjmp bug: 'frame' and 'newf' locals in vm_execute needed volatile qualifiers, since they're reassigned after setjmp() and the C standard leaves their post-longjmp value undefined otherwise (GCC's -Wclobbered caught this) — was causing corrupted error messages and cascading state after any caught error

## 2026-07-16 08:50
added interactive REPLs to both ashc and ashvm; ashc auto-wraps each line in try/catch for safe error recovery, ashvm uses a separate C-level recovery jmp_buf (vm_set_repl_recovery) instead of synthetic try/catch, to avoid reintroducing the variable-scoping bug from earlier; both support multi-line input via brace counting, persist variables/functions across lines, syntax errors still end the session (known limitation, consistent with both engines treating syntax errors as fatal)

