# Changelog

All notable changes to QuantoScript will be documented in this file.

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
- Release audit completed (96/100 score)

### Documentation
- Complete API documentation (DOCS.md)
- Release audit report (docs/V1_RELEASE_REPORT.md)
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
- **Async/await is broken** — `run_all()` hangs with 2+ tasks, non-deterministic crashes under load. The task queue (`stdlib/async.qs`) is not functional for production use. Individual `spawn()`/`run()`/`result()` work for single tasks. See `ASYNC_VALIDATION.md` for full audit.
