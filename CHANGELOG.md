# Changelog

All notable changes to QuantoScript will be documented in this file.

## [Unreleased]

### Bug Fixes
- WebSocket client now supports secure `wss://` connections (OpenSSL TLS handshake) and connects over IPv6 as well as IPv4; `ws_connect` now reports that it expects a `ws://` or `wss://` URL.
- `regex_match` / `regex_find` / `regex_replace` now use a real POSIX regular-expression engine on Windows (via the optional `libsystre`/`libregex` package) instead of literal substring matching, falling back to substring matching only where POSIX regex is unavailable. `regex_replace` previously performed a literal substring replacement, and its documented argument order was wrong.
- `--sandbox <dir>` now confines every filesystem built-in (`sys_read`, `sys_write`, `sys_exists`, `sys_mkdir`, `sys_remove`, `sys_rename`, `sys_is_dir`, `sys_list_dir`, `sys_chdir`) and `cwd()` to the sandbox root; absolute paths and `..` escapes are rejected with `access denied: sandbox mode`.
- Map `extend` now merges a map argument into a map receiver instead of raising a syntax error.
- `repeat key, value -> map` now yields `[key, value]` pairs when iterating over maps.
- Lambdas with braced multi-statement bodies can now be passed to calls and executed.
- Strings may now contain the same quote character that delimits them (for example `"i use " inside this string."` or `'i use ' inside this string.'`) on the interpreter, the bytecode VM, and compiled binaries. Previously the first inner matching quote was treated as the closing quote, which raised `unexpected text after expression` in the interpreter. A quote is now treated as the closing quote only when it is followed by the end of the line, a comment, or a token that can continue the expression (`,`, `)`, `]`, `.`, an operator, `in`/`and`/`or`/`import`, ...).

### Tooling
- The `Makefile` and `qs compile` now probe for POSIX regex (`<regex.h>`) and link `libregex` only when it is available; CI installs `mingw-w64-*-libsystre` so released Windows binaries include real regex support.

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
