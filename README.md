<div align="center">

# QuantoScript

**A small, readable scripting language with a dual execution engine.**

Dynamic typing · `try`/`oops` exceptions · classes with shared identity · a tree-walk interpreter *and* a bytecode VM · an optional native-C compiler.

[![CI](https://github.com/PySudo/QuantoScript/actions/workflows/ci.yml/badge.svg)](https://github.com/PySudo/QuantoScript/actions/workflows/ci.yml)
[![version](https://img.shields.io/badge/version-1.0.0-blue.svg)](CHANGELOG.md)
[![language](https://img.shields.io/badge/written%20in-C99-00599C.svg)](src/quanto.c)
[![license](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![platforms](https://img.shields.io/badge/platforms-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey.svg)](#building-from-source)

[Documentation](DOCS.md) · [Examples](examples/) · [Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md)

</div>

---

## Table of Contents

- [Why QuantoScript?](#why-quantoscript)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Language Tour](#language-tour)
- [Execution Engines](#execution-engines)
- [Compiled Bytecode (`.qvm`)](#compiled-bytecode-qvm)
- [Native Compiler](#native-compiler)
- [Standard Library](#standard-library)
- [Command-Line Interface](#command-line-interface)
- [Package Manager](#package-manager)
- [Building from Source](#building-from-source)
- [Project Structure](#project-structure)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)

---

## Why QuantoScript?

QuantoScript is a small, embeddable scripting language designed for readability and
simplicity — think Python's clarity with a simpler, brace-based syntax. It ships with
two interchangeable execution backends (a tree-walk interpreter for fast iteration and
a bytecode VM for performance) plus an optional native-C compiler for compute-heavy
numeric code.

| | |
|---|---|
| **Braces, not indentation** | Blocks are delimited with `{ }` — no significant whitespace. |
| **Friendly keywords** | `maybe` instead of `elif`, `oops` instead of `except`. |
| **1-based indexing** | Lists and strings start at index `1`. |
| **Shared object identity** | Class instances are references, not copies. |
| **Dual engine** | The same program runs on the interpreter or the bytecode VM with identical output. |
| **Native fast-path** | Arithmetic-heavy code can compile to C for a ~10× speedup. |
| **Single-file core** | The whole language is one C99 amalgamation — easy to embed and vendor. |

---

## Installation

The fastest way to get QuantoScript is the one-line installer. It downloads a
prebuilt, self-contained binary (OpenSSL bundled in — no dependencies to install),
places it under `~/.quanto`, and adds it to your `PATH`. No compiler and no `sudo`
required.

### macOS / Linux

```bash
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh
```

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1 | iex
```

Then open a new terminal and run `qs --help`.

> Prebuilt binaries are provided for macOS (Apple Silicon), Linux (x86-64 and
> ARM64), and Windows (x86-64). On an Intel Mac the installer will ask you to
> [build from source](#building-from-source) until a prebuilt Intel binary is
> published.

<details>
<summary><b>Installer options</b></summary>

**macOS / Linux** (`install.sh`)

```bash
# Install a specific version
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh -s -- --version 1.0.0

# Custom install prefix (default: ~/.quanto)
QUANTO_INSTALL="$HOME/.local" curl -fsSL .../install.sh | sh

# Don't touch your shell profile
NO_MODIFY_PATH=1 curl -fsSL .../install.sh | sh

# Uninstall
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh -s -- --uninstall
```

**Windows** (`install.ps1`)

```powershell
# Install a specific version
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1))) -Version 1.0.0

# Uninstall
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1))) -Uninstall
```

Prefer to build it yourself? See [Building from Source](#building-from-source).

</details>

---

## Quick Start

Already installed? Skip straight to Hello World. Otherwise build the interpreter with
`make` (see [Building from Source](#building-from-source) for OpenSSL notes).

```bash
qs examples/full_tour.qs   # run a script
qs                         # or drop into the REPL
```

**Hello, World!**

```qscript
print("Hello, World!")

func greet(name) {
    return "Hello, " + name + "!"
}

print(greet("QuantoScript"))
```

```bash
qs hello.qs          # tree-walk interpreter
qs vm hello.qs       # bytecode VM (identical output)
```

---

## Language Tour

### Variables and Types

```qscript
x       = 42                # integer
pi      = 3.14              # float
name    = "QS"              # string
flag    = true              # boolean
nothing = null              # null
items   = [1, 2, 3]         # list
user    = {"name": "QS"}    # map
```

### Functions and Lambdas

```qscript
func add(a, b) {
    return a + b
}
print(add(3, 4))            # 7

double = fn(x) -> x * 2     # lambda expression
print(double(5))            # 10
```

### Control Flow

```qscript
if x > 10 {
    print("big")
} maybe x > 5 {             # "maybe" == "else if"
    print("medium")
} else {
    print("small")
}

repeat 5 { i = i + 1 }      # counted loop
while x < 100 { x = x * 2 } # conditional loop
```

### Exception Handling — `try` / `oops`

```qscript
func safe_div(a, b) {
    if b == 0 {
        oops("division by zero")   # raise
    }
    return a / b
}

try {
    print(safe_div(10, 0))
} oops e {                          # catch, binding the error to `e`
    print("caught: " + e)
}
```

### Classes with Shared Identity

```qscript
class Person {
    init(name) {
        self.name = name
    }

    greet() {
        print("Hi, I'm " + self.name)
    }
}

p = Person("Alice")
p.greet()           # Hi, I'm Alice

c = p               # c and p reference the SAME object
c.name = "Bob"
p.greet()           # Hi, I'm Bob
```

> **Note:** Method bodies must use multi-line syntax — one statement per line.

### Collections

```qscript
nums = [3, 1, 4, 1, 5]
print(len(nums))            # 5
nums.push(9)
print(nums.contains(4))     # true

user = {"name": "QS", "version": 1}
print(user["name"])         # QS
print(user.keys())          # [name, version]
```

**Built-in methods**

| Type | Methods |
|------|---------|
| String | `upper` · `lower` · `title` · `len` · `contains` · `replace` · `startsWith` · `endsWith` · `split` |
| List | `len` · `push` / `append` · `pop` · `contains` · `index` · `remove` |
| Map | `keys` · `values` · `items` · `has` · `remove` · `len` |

See [DOCS.md](DOCS.md) for the complete language reference.

---

## Execution Engines

The same source runs on either backend and produces identical output.

| Engine | Command | Best for |
|--------|---------|----------|
| Tree-walk interpreter | `qs program.qs` | Development, debugging, quick scripts |
| Bytecode VM | `qs vm program.qs` | Better runtime performance |
| Native C compiler | `qs native program.qs` | ~10× speedup on arithmetic-heavy code |

---

## Compiled Bytecode (`.qvm`)

Compile QuantoScript to a stable binary bytecode format for faster startup and
distribution without shipping source.

```bash
qs build program.qs                 # -> program.qvm
qs build program.qs -o custom.qvm   # custom output path

qs vm program.qvm                   # run compiled bytecode
qs vm --dump-bytecode program.qs    # disassemble for debugging
```

The `.qvm` format (**version 4**) includes a magic-header check, version validation,
CRC32 source checksums, and comprehensive handling of malformed files. See
[docs/QVM_FORMAT.md](docs/QVM_FORMAT.md) for the full specification.

---

## Native Compiler

For arithmetic-heavy workloads, QuantoScript can emit C and compile it to a native
binary:

```bash
qs native program.qs        # emit a_native.c
gcc -O2 a_native.c -o program_native
./program_native
```

**Supported:** integer arithmetic, `if`/`else`, `while`, `repeat`, variable assignment,
`print`.
**Not supported:** functions, strings, lists, maps, closures, classes, exceptions.

---

## Standard Library

Import modules with `from "<module>" import <names>`. Public modules live in `stdlib/`;
internal runtime hooks (`sys_*`) stay hidden.

```
User code  →  stdlib modules  →  native runtime  →  OS
```

```qscript
from "stdlib/http.qs"      import get, post
from "stdlib/json.qs"      import parse_json, to_json
from "stdlib/os.qs"        import run, capture, cwd, chdir, exists
from "stdlib/fs.qs"        import read, write, list_dir
from "stdlib/text.qs"      import lower, upper, split
from "stdlib/time.qs"      import now, localtime
from "stdlib/log.qs"       import log_info, log_error
from "stdlib/websocket.qs" import connect, send, recv, close
```

**Examples**

```qscript
# HTTP
from "stdlib/http.qs" import get
resp = get("https://httpbin.org/get")
print(resp)

# JSON
from "stdlib/json.qs" import parse_json
data = parse_json("{\"key\": \"value\"}")

# Process execution
from "stdlib/os.qs" import run, capture, exists
run("echo Hello")                   # returns exit code
version = capture("git --version")  # returns stdout
print(exists("gcc"))                # true / false
```

| Module | Purpose |
|--------|---------|
| `core.qs` | Core helpers and assertions |
| `math.qs` | Numeric utilities |
| `text.qs` | String processing |
| `fs.qs` | File system operations |
| `os.qs` | Process execution & working directory |
| `http.qs` | HTTP client (`get`, `post`, `put`, `delete`, `patch`) |
| `net.qs` | Low-level networking |
| `websocket.qs` | WebSocket client |
| `json.qs` | JSON parse / stringify |
| `time.qs` | Time and clock operations |
| `log.qs` | Structured logging |
| `random.qs` | Pseudo-random numbers |
| `async.qs` | Task queue *(experimental — see [Known Limitations](#known-limitations))* |

At runtime, stdlib modules are resolved via the `QUANTO_HOME` environment variable.
Run `qs home` to print the configured location. See
[docs/LANGUAGE_ARCHITECTURE_AUDIT.md](docs/LANGUAGE_ARCHITECTURE_AUDIT.md) for full
architecture details.

---

## Command-Line Interface

```
qs                          interactive REPL
qs <file.qs>                run with the tree-walk interpreter
qs run <file.qs|.qvm>       run with the bytecode VM
qs vm <file.qs|.qvm>        run with the bytecode VM (alias for run)
qs build <file.qs> [-o out.qvm]   compile to .qvm bytecode
qs native <file.qs>         emit native C (a_native.c)
qs compile <file.qs> [out]  compile to a native binary
qs init                     scaffold a new project
qs check <file.qs>          validate syntax without executing
qs fmt <file.qs>            format source code
qs lint <file.qs>           report common mistakes
qs doc <file.qs>            generate documentation from comments
qs profile <file.qs>        profile function calls
qs test [directory]         run *_test.qs files
qs install <github-url>     install a package
qs list                     list installed packages
qs remove <package>         remove a package
qs home                     print the QUANTO_HOME directory
qs version                  print the version
```

**Flags**

```
--dump-bytecode     print compiled bytecode to stderr
--trace             print a per-instruction VM trace
--sandbox [path]    restrict file access to the given path
```

---

## Package Manager

QuantoScript has a built-in, git-backed package manager. Packages are cloned from
GitHub into a local `packages/` directory, and their `quanto.json` dependencies are
resolved recursively.

```bash
qs install https://github.com/owner/repo
qs list
qs remove repo
```

```qscript
from "packages/owner_repo/main.qs" import something
```

Scaffold a project with a manifest via `qs init`, which creates `main.qs`,
a `test/` directory, and a `quanto.json`.

---

## Building from Source

QuantoScript is a single C99 amalgamation (`src/quanto.c`). It requires a C compiler
and **OpenSSL** (used by the HTTP/WebSocket stack). Networking libraries differ per
platform.

### Prerequisites

| Platform | Toolchain | OpenSSL |
|----------|-----------|---------|
| macOS | Xcode Command Line Tools (`clang`) or `gcc` | `brew install openssl@3` |
| Linux | `gcc` / `clang` | `libssl-dev` (Debian/Ubuntu) or `openssl-devel` (Fedora) |
| Windows | MSYS2 / MinGW-w64 `gcc` | `pacman -S mingw-w64-ucrt-x86_64-openssl` |

### macOS

Homebrew installs OpenSSL outside the default search path, so point the compiler at it:

```bash
SSL=$(brew --prefix openssl@3)
cc -std=c99 -O2 -Isrc -I"$SSL/include" src/quanto.c -o qs \
   -L"$SSL/lib" -lssl -lcrypto -lpthread -lm
```

### Linux

```bash
gcc -std=c99 -O2 -Isrc src/quanto.c -o qs \
    -lssl -lcrypto -lpthread -ldl -lm
```

### Windows (MSYS2 / MinGW-w64)

```bash
gcc -std=c99 -O2 -Isrc src/quanto.c -o qs.exe \
    -lssl -lcrypto -lws2_32 -lwinhttp -lwininet -lcrypt32
```

### Using Make (recommended)

The `Makefile` auto-detects OpenSSL (Homebrew on macOS, `pkg-config` elsewhere):

```bash
make          # build build/qs
make test     # build and run the regression suite
make clean    # remove build artifacts
```

Useful overrides:

```bash
make OPENSSL_DIR=/path/to/openssl   # point at a specific OpenSSL prefix
make STATIC_SSL=1                   # statically link OpenSSL (self-contained binary)
make CC=clang                       # choose the compiler
```

### Using CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Optional: self-contained binary and packaging
cmake -S . -B build -DQS_STATIC_OPENSSL=ON && cmake --build build
cpack --config build/CPackConfig.cmake      # produce a .tar.gz / .zip
```

### Optional: Python embedding

Building with `-DQS_PYTHON` and the Python headers enables the Python FFI examples.

---

## Project Structure

```
src/                C source (single-file amalgamation)
  quanto.c          Entry point that includes the parts below
  parts/            Language modules (.inc): parser, VM, compiler, runtime, CLI, …
stdlib/             Standard library modules (.qs)
examples/           Example programs (see full_tour.qs)
tests/              Regression and stress test suite
docs/               Language & format documentation, audit reports
tools/              Benchmarks, CMake helpers, packaging metadata
scripts/            Build and install scripts
build/              Build output (gitignored)
```

---

## Known Limitations

This is a **v1.0.0** release. The following are known and tracked:

- **Closures** with captured variables do not work — inner functions capturing outer
  locals return `null`.
- **Class method bodies** must be written one statement per line (multi-line syntax).
- **Return type annotations** are parsed but not yet enforced.
- **Async/await is not functional.** The `stdlib/async.qs` task queue hangs with two or
  more tasks and can crash non-deterministically under load. Single-task
  `spawn`/`run`/`result` work. See [docs/ASYNC_VALIDATION.md](docs/ASYNC_VALIDATION.md).
- The tree-walk parser does not support single-line `try`/`oops` syntax.
- **Python embedding** requires the `-DQS_PYTHON` build flag and Python headers.

---

## Roadmap

| Version | Focus |
|---------|-------|
| **v0.2.0** | Working closures, richer string/map methods |
| **v0.3.0** | Async/await, broader native-compiler coverage |

---

## Contributing

Issues and pull requests are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for
guidelines and [CHANGELOG.md](CHANGELOG.md) for release history. When submitting
changes, please run the regression suite first:

```bash
make test
```

---

## License

Released under the [MIT License](LICENSE).
