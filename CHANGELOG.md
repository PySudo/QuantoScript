# Changelog

All notable changes to QuantoScript will be documented in this file.

## [Unreleased]

### Features
- New operators across every engine (interpreter, `qs vm`, `qs build`/qvm, and native `qs native`): `??` (null coalescing) returns its left operand unless it is `null`, in which case it returns the right operand; `?.` and `?[` (optional chaining) return `null` instead of raising when the receiver is `null` or the field, method, or key is missing, so `user?.name`, `user?.greet()`, `user?["id"]`, and `data?.missing` (on a map) are safe and chains stop at the first missing or null link.
- `qs lsp` starts a Language Server Protocol server on stdio (JSON-RPC 2.0 with `Content-Length` framing). It supports the initialize/shutdown handshake, full document sync (`didOpen`/`didChange`/`didSave`/`didClose`), diagnostics via `textDocument/publishDiagnostics` (reusing `qs check`), and keyword `textDocument/hover`. The bundled VS Code extension in `vscode/` now connects to it for live diagnostics and hover.
- Maps now use a hash index for key lookup instead of a linear scan, in every engine (interpreter, `qs vm`, `qs build`, and native `qs native`). `has`, `get`, `get(key, default)`, `keys`, `items`, and `remove` keep insertion order for iteration, but `has`/`get`/`remove` are constant-time on average. Existing programs and outputs are unchanged.
- `qs native` now recognizes the common built-in map and string methods (`has`, `get`, `keys`, `values`, `items`, `split`, `join`, `contains`, `indexOf`, `substring`, `startsWith`, `endsWith`, `trimStart`, `trimEnd`, `title`, `replace`, `repeat`, `reverse`) and emits direct calls to specialized C helpers instead of the generic `qs_method_call` dispatcher. Each call keeps a runtime type guard and falls back to the generic path when the receiver is not the expected type, so behaviour is unchanged. `len(...)` is also inlined for strings, lists, and maps.

### Features
- WebAssembly build via Emscripten. `make wasm` compiles the full interpreter, bytecode VM, and native compiler to a `.wasm` module (`build/qs_node.js` + `build/qs_node.wasm`). A prebuilt runtime ships in `runtime/wasm/` and is installed alongside the binary, so users run programs without building anything: `qs wasm program.qs`, `qs wasm vm program.qs`, `qs wasm build program.qs -o out.qvm`, `qs wasm run out.qvm`. The `qs wasm` command finds the installed runtime and drives Node (override the interpreter with `QS_NODE`). Under Node the virtual filesystem is backed by NODERAWFS, so file access works. The core language is unchanged; only the platform layer differs. `make wasm-test` runs every test and example through both the native binary and the WASM build and reports parity. On the WebAssembly target the platform-dependent features are unavailable and report a clear runtime error: live networking (`http`, `websocket`), FFI (`ffi_*`), POSIX regex, and process spawning (`sys_run`, `sys_capture`), so a handful of network/regex tests differ by design.

### Bug Fixes
- `len(map)` now works on the interpreter; it previously failed with `len needs a string or list`. All three engines accept maps and report `len needs a string, list, or map` for other types.

### Performance
- New `map` and `strm` benchmarks in the benchmark suite.
- Native string-method benchmark: 81 ms -> 74 ms. Native map benchmark: 2.75 s -> 2.52 s; map access is still dominated by value cloning on every `get`.
- Native method calls, indexing, slicing, and membership tests on plain variables now borrow the stored value instead of deep-cloning it on every access. `data.get(...)`, `base.split(...)`, `xs[0]`, `xs[1..2]`, and `x in xs` read in place, so map- and string-heavy loops no longer copy the receiver each iteration.
- Aggregate variables are no longer constant-folded into a freshly rebuilt literal at every use; only scalar constants are folded. Loops that read a map or list assigned once no longer rebuild that literal on each access.
- Native map benchmark: 2.76 s -> 79 ms (median of alternating runs; 20 000 `get`/`has` calls on a 100-key map). String-method benchmark stays around 60 ms. All benchmark, example, and regression outputs are unchanged.
- Map key lookup is now constant-time on average. A 100 000-lookup loop over a 2 000-key map drops from ~1.35 s to ~0.13 s in native code (~10x), and the same speedup applies to the interpreter and both bytecode engines.

## [2.1.0] - 2026-10-20

### Features
- `qs native` now promotes script variables that provably hold only integers to bare C `long` locals. Loops such as `i = i + 1` compile to a register increment instead of a string-keyed environment lookup plus boxed-value arithmetic, which makes integer-heavy programs several times faster.
- `qs native` emits an integer-only variant of every function whose parameters and body are entirely integer. Calls made in integer context go straight to the `long` variant, skipping the boxed calling convention. Recursive integer code such as `fib` benefits the most.
- `s += expr` on a string now appends into the existing buffer instead of cloning the left side, concatenating, and storing the result. Building a large string in a loop is no longer quadratic.
- `list.append` takes a fast path in `qs native` that pushes the argument directly, and variable assignment does a single name lookup instead of two.

### Bug Fixes
- In-place method calls (`xs.append(x)`, `xs.sort()`, …) now write the result back only when the receiver is a plain variable. Previously a receiver such as `f().items` or `xs[0]` could overwrite an unrelated variable.
- The bytecode VM now writes back in-place method results for calls that are part of a larger expression, not only for bare statement calls, matching the interpreter and `qs native`.
- Calling a mutating method on a definition (for example `MAX.append(1)`) now reports the standard `definition '…' already exists and cannot be changed` error on the bytecode VM as well.

### Performance
- `fib(30)`: 42.7 ms vs 354.6 ms for CPython (8.3×).
- Tight integer loop: 41.0 ms vs 1128 ms for CPython (27.5×).
- String building: 141.3 ms vs 158.4 ms (1.12×).
- List appends: 208.3 ms vs 219.5 ms (1.05×).

## [2.0.1] - 2026-10-16

### Bug Fixes
- The interpreter now evaluates every term of an additive expression. A `+`/`-` chain with more than two operands (for example `1 + 2 + 3`) previously stopped after the first operator on the tree-walk interpreter, which then mis-reported the line as a syntax error; the bytecode VM and `qs native` were already correct, and now all three engines agree.
- Released archives now include the C runtime sources (`src/`), so `qs native` and `qs compile` work from a fresh install. The release package previously omitted `src/`, causing `qs native` to fall back to a stale or missing runtime directory and fail with undeclared-symbol errors.
- `qs native`/`qs compile` now validate a discovered runtime directory before using it: it must contain both `quanto.c` and `src/parts/native_runtime.inc`. A mismatched directory (for example a leftover `src` copy) is skipped in favour of a valid one, and when none is found the error explains how to point at the sources with `QS_SOURCE_DIR`.
- The regression runner no longer fails on tests that depend on external services (the live HTTP and WebSocket examples). Those tests are listed in `tests/nondeterministic_tests.txt` and reported separately, so intermittent third-party server errors no longer break CI.

## [2.0.0] - 2026-10-15

### Breaking Changes
- List comprehensions now use `repeat` syntax instead of `for ... in`: `[expr repeat var -> coll]` (e.g. `[x * 2 repeat x -> nums]`), `[expr repeat -> coll]` where the leading name doubles as the loop variable (e.g. `[i repeat -> range(3)]`), and `[expr repeat count]` (e.g. `[0 repeat 3]`). An optional trailing `if cond` still filters. The collection may be a list, string, map, or number (counting `1`..`N`), matching the `repeat` statement. The old `[expr for var in coll]` form is no longer accepted.

### Features
- The `qs native` backend now compiles the whole language — numbers, strings, lists, maps, control flow, functions, classes, exceptions, imports, and comprehensions — to C, and the resulting binary behaves like the tree-walk interpreter and the bytecode VM. Previously the native path handled only a small subset (integer arithmetic, `if`/`while`/`repeat`, variable assignment, and `print`).

### Bug Fixes
- The bytecode VM and `qs native` now implement list comprehensions. The VM previously crashed on them because the AST compiler had no `AST_LIST_COMP` case; the tree-walk interpreter already handled them.
- A malformed comprehension (for example `[1 * 2 repeat -> 5]`, where the shorthand has no leading name) now reports a clear `SyntaxError: comprehension needs a variable name before '->'` on all three engines instead of crashing or producing garbage.
- A closing quote immediately before the keyword `if` now ends the string literal (e.g. `[c for c in "abc" if c != "b"]`-style expressions). Previously `if` was missing from the closing-quote continuation list, so a comprehension filter following a string collection swallowed the rest of the line into the string.

## [1.1.3] — 2026-10-14

### Features
- Added `get(key[, default])` to maps: returns the value for `key`, the `default` when the key is missing, or `null` when no default is supplied. This avoids testing with `has` before indexing; reading a missing key with `m["key"]` still throws.

### Bug Fixes
- User-defined class methods now take precedence over built-in collection methods on the tree-walk interpreter, matching the bytecode VM. Previously a class with a method named like a built-in (for example `get`) raised `get works only on maps` on the interpreter while working on the VM.

### Documentation
- Documented the map `get` method in the Maps guide and simplified the `.env` loading examples to use `cfg.get("API_KEY", "not set")`.

## [1.1.2] — 2026-10-14

### Features
- Added `parse_env(text)` and `load_env(path)` to `stdlib/core.qs` for reading dotenv-style `.env` files. Blank lines and `#` comments are skipped, spaces around the `=` are trimmed, and `=` characters inside a value are preserved. `load_env` returns an empty map when the file does not exist, and missing keys throw, so callers test with `has` first.

### Documentation
- Documented list comprehensions (`[expr for x in coll if cond]`) in the Lists guide and removed them from the "not yet supported" list; they have always worked over lists, strings, and maps. Control Flow now notes that QuantoScript has no C-style `for` loop.
- Fixed the WebSocket examples to connect with `wss://echo.websocket.org` and consume the server's greeting on connect, and replaced an unsupported C-style `for` loop with `repeat`.

## [1.1.1] — 2026-10-13

### Bug Fixes
- WebSocket client now supports secure `wss://` connections (OpenSSL TLS handshake) and connects over IPv6 as well as IPv4; `ws_connect` now reports that it expects a `ws://` or `wss://` URL.
- `regex_match` / `regex_find` / `regex_replace` now use a real POSIX regular-expression engine on Windows (via the optional `libsystre`/`libregex` package) instead of literal substring matching, falling back to substring matching only where POSIX regex is unavailable. `regex_replace` previously performed a literal substring replacement, and its documented argument order was wrong.
- `--sandbox <dir>` now confines every filesystem built-in (`sys_read`, `sys_write`, `sys_exists`, `sys_mkdir`, `sys_remove`, `sys_rename`, `sys_is_dir`, `sys_list_dir`, `sys_chdir`) and `cwd()` to the sandbox root; absolute paths and `..` escapes are rejected with `access denied: sandbox mode`.
- Map `extend` now merges a map argument into a map receiver instead of raising a syntax error.
- `repeat key, value -> map` now yields `[key, value]` pairs when iterating over maps.
- Lambdas with braced multi-statement bodies can now be passed to calls and executed.
- Strings may now contain the same quote character that delimits them (for example `"i use " inside this string."` or `'i use ' inside this string.'`) on the interpreter, the bytecode VM, and compiled binaries. Previously the first inner matching quote was treated as the closing quote, which raised `unexpected text after expression` in the interpreter. A quote is now treated as the closing quote only when it is followed by the end of the line, a comment, or a token that can continue the expression (`,`, `)`, `]`, `.`, an operator, `in`/`and`/`or`/`import`, ...).
- `regex_match` / `regex_find` / `regex_replace` now accept a leading inline case-insensitive flag (`(?i)`), translating it to the portable `REG_ICASE` flag. Previously `(?i)` was accepted by libsystre on Windows but rejected by glibc's POSIX engine on Linux/macOS, which made the same pattern behave differently per platform.

### Tooling
- Windows binaries now link the POSIX-regex dependency chain (`libsystre` → `libtre` → `gettext` → `iconv`) statically, so `qs.exe` has no extra DLL dependencies; together with the static OpenSSL link this keeps released Windows binaries self-contained. `qs compile` likewise emits fully static Windows binaries. The `Makefile`, `qs compile`, and CI link the regex libraries only where `<regex.h>` is available (`mingw-w64-*-libsystre`).
- Windows CI copies the OpenSSL DLLs the CI build links against next to `qs.exe` before running the regression suite, so the MinGW binary loads its dependencies without relying on `PATH`. The release pipeline now also verifies that the Windows binary imports no non-system DLLs.

## [1.1.0] — 2026-10-13

### Highlights
- Unified function-variable scoping between the tree-walk interpreter and the bytecode VM. Functions defined in the main program now read and write the global environment directly (mirroring the VM's compiled `OP_STORE_VAR` path), while imported/module functions keep private frames.
- Centralized the product version into a single `QS_VERSION` definition consumed by the CLI, project template, and HTTP user agent.

### Features
- Added the `morph(value, type)` builtin for converting values between types (`int`, `float`, `string`, `bool`, `list`, `map`). Type names may be written bare (`morph("5", int)`) or as strings (`morph("5", "int")`), aliases (`integer`, `number`, `str`, `dict`, ...) are accepted, and the bare type names now work as values with `isinstance`.
- Added the `stdlib/url.qs` module with `url_encode` and `url_decode` for percent-encoding query values.
- Added `sleep(milliseconds)` to `stdlib/time.qs` (backed by the new `sys_sleep` builtin) for pacing loops such as polling bots.

### Bug Fixes
- Interpreter/VM scope parity: non-parameter assignments in main-program functions now write through to the global environment; parameters and imported-function locals stay private.
- Fixed a garbage (uninitialized) block in class-method collection that could crash class parsing.
- `sys_*` builtins now report `'<name>' requires native mode` instead of silently failing when used without native mode.
- Fixed f-strings with trailing literal text after the final interpolation.
- Fixed VM handling of single-line `if`/`else` bodies (`ast_parse_block_body`).
- `try`/`oops` now behave identically on all three execution paths for every layout: fully single-line (`try { A } oops e { B }`), inline try with a multi-line oops block, and multi-line try with an inline oops block.
- JSON parsing now decodes `\uXXXX` escapes (including UTF-16 surrogate pairs) into real UTF-8 characters instead of leaving the literal `uXXXX` text.
- Fixed a use-after-free when running a `.qvm` image (`run_qvm_file`).
- HTTP requests now percent-encode characters that are invalid in a URL (spaces, non-ASCII bytes, backslashes) instead of sending a malformed request line, which previously caused servers to reply `400 Bad Request`.
- HTTP responses now report the real transport error code in the `error` key when a request fails (previously it was always `0`), making timeouts and TLS failures diagnosable.
- `qs compile` now finds the C runtime sources (`quanto.c` and `src/parts/`) relative to the installed binary, `QUANTO_HOME`, a `QS_SOURCE_DIR` environment variable, and the current directory tree (previously it only searched the working directory), and reports a clear error when they are missing. Release archives and the installer now ship the `src/` runtime so `qs compile` works from a normal installation. The generated build no longer prints the runtime's C compiler warnings, and the underlying unused-parameter/variable and format-truncation warnings were fixed. It also no longer creates an empty `build/` directory or leaves the intermediate generated `.c` file behind (the `.c` is kept only when the build fails, for debugging).
- On Windows, non-ASCII output (for example Persian or other UTF-8 text) now prints correctly: the CLI switches the console to the UTF-8 code page on startup and restores the previous one on exit.

### Tooling
- Regression runner now normalizes nondeterministic wall-clock values (uptime, timestamps) before comparing outputs across paths.
- Version is now a single source of truth (`QS_VERSION`), overridable at build time via `make VERSION=x.y.z`.
- Removed leftover audit/validation scratch scripts so the repository ships only the maintained test suite.

## [1.0.0] — 2026-07-11

### Highlights

First stable release of QuantoScript. A small, readable scripting language with a dual execution engine: tree-walk interpreter and bytecode VM.

### New Language Features
- Single-line function bodies: `func f() { return 1 }` now works on all execution paths
- Single-line class method bodies: `class Foo { init() { self.x = 1 } }`
- Class methods calling external helper functions
- Recursion from class methods
- Nested while/if/repeat loops inside class methods

### VM Improvements
- Fixed method body splitting for nested blocks (while inside while, etc.)
- Fixed `quote_can_close` to accept name-start characters as valid terminators
- Rewrote `split_text_to_program` as a deterministic state machine
- Fixed `return` statement handling in method body splitting
- Class method body parsing now consistent between interpreter and VM

### Parser Fixes
- Fixed nested block body extraction (brace depth tracking)
- Fixed single-line function body parsing in interpreter
- Fixed `ast_parse_block_body` to use shared splitter

### Class System Fixes
- Class methods can now call external functions
- Self field assignments inside loops work correctly
- Nested while loops in class methods work on all paths

### Import Fixes
- Improved import path resolution

### Infrastructure
- Added automated regression test runner (132 tests, 396 executions)
- GitHub Actions CI with Windows/macOS/Linux support

### Documentation
- Complete API documentation (DOCS.md)
- CI status badge added to README

### Known Limitations
- Tree-walk parser does not support single-line try/oops syntax
- Single-line `if` bodies not supported in interpreter (VM handles them)
- Async/await is not functional for production use
- Some string methods only available via VM dispatch

## [0.1.0-beta] — 2026-07-01

### Added

#### Core Language
- Integer, float, string, boolean, null, list, and map types
- Variables with dynamic typing
- Arithmetic operators (+, -, *, /, %)
- Comparison operators (==, !=, >, <, >=, <=)
- Logical operators (and, or, not)
- String concatenation and interpolation (f-strings)
- If/else/maybe conditional blocks
- While and repeat loops
- Functions with recursion and multiple parameters
- Lambda expressions
- try/oops exception handling with error variable binding
- 1-based indexing for lists and strings

#### Bytecode VM
- AST-based bytecode compiler and virtual machine
- 41 opcodes covering all language features
- Generic method dispatch for string/list/map operations
- Unified exception propagation via `vm_raise()`
- Heap-allocated sub-VMs for safe recursive function calls

#### String Methods
- upper, lower, title, len, contains, replace, startsWith, endsWith, split

#### List Methods
- len, push/append, pop, contains, index, remove

#### Map Methods
- keys, values, items, has, remove, len

#### Standard Library
- Math utilities (math.qs)
- File system operations (fs.qs)
- HTTP client (http.qs)
- JSON parse/stringify (json.qs)
- Networking (net.qs)
- Text processing (text.qs)
- Time operations (time.qs)
- Logging (log.qs)
- WebSocket support (websocket.qs)
- Process execution (os.qs)

#### Tools
- Interactive REPL
- Differential fuzz testing framework
- Collection stress tests
- Recursion stress tests
- Exception propagation tests

### Fixed
- Unified 1-based indexing across interpreter and VM
- Double-negation bug in AST parser (e.g., `x = -87`)
- Out-of-range list access now throws catchable exceptions
- Recursive function argument order (forward-order push)
- Function argument count validation in VM
- try/oops error variable binding in bytecode compiler

### Known Limitations
- Tree-walk parser does not support single-line try/oops syntax
- Built-in argument count validated at parse time (interpreter) vs runtime (VM)
- Some string methods (upper, lower, etc.) available only via the VM's dispatch_method
- **Async/await is broken** — `run_all()` hangs with 2+ tasks, non-deterministic crashes under load. The task queue (`stdlib/async.qs`) is not functional for production use. Individual `spawn()`/`run()`/`result()` work for single tasks.
