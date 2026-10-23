[« Previous: Style and Limits](20-style-and-limits.md) · [Guide Index](README.md)

# 21. WebAssembly

QuantoScript compiles to WebAssembly, so a program can run inside Node.js (and,
with the right runtime shims, inside a browser or any other WASM host). The
source language is exactly the same as on every other engine; only the platform
layer changes.

---

## Building

A prebuilt runtime (`runtime/wasm/qs_node.js` + `qs_node.wasm`) ships with the
project and is installed alongside the `qs` binary, so **most users never build
anything** — they just run `qs wasm program.qs` (see below).

To build it yourself you need the [Emscripten SDK](https://emscripten.org) on
`PATH`, then use the `wasm` target:

```bash
emsdk install latest
emsdk activate latest
make wasm
```

This (re)generates `build/qs_node.js` + `build/qs_node.wasm`. The `make wasm`
target is for maintainers refreshing the shipped runtime; the release/install
scripts copy the result into `<home>/wasm/`.

`make wasm-debug` builds an unoptimised module with assertions for debugging the
runtime itself. The build is cross-platform: it is just `emcc`, so it works the
same on Linux, macOS, and Windows (under a POSIX shell such as MSYS2, WSL, or
Git Bash).

---

## Running

The easiest way is the built-in `qs wasm` command, which finds the installed
WebAssembly runtime and runs your program under Node for you:

```bash
qs wasm program.qs          # interpreter
qs wasm vm program.qs       # bytecode VM
qs wasm build program.qs -o out.qvm
qs wasm run out.qvm
```

The `qs wasm` command needs a [Node.js](https://nodejs.org) executable. It uses
`QS_NODE` if set, otherwise `node` on `PATH`, and finally a few common install
locations (`C:\Program Files\nodejs`, `/usr/local/bin`, `/opt/homebrew/bin`). If
Node is not installed, install it first or set `QS_NODE` to its full path.

You can also drive the loader directly. Under Node the filesystem is backed by
NODERAWFS, so real files can be read and written:

```bash
node build/qs_node.js program.qs          # interpreter
node build/qs_node.js vm program.qs       # bytecode VM
node build/qs_node.js build program.qs -o out.qvm
node build/qs_node.js run out.qvm
```

---

## Self-contained bundles (`qs wasm bundle`)

When you need to ship a program to someone who only has Node.js installed (no
`qs` binary, no C compiler, no Emscripten), bundle it:

```bash
qs wasm bundle program.qs                # creates ./program_wasm/
qs wasm bundle program.qs -o dist/app    # custom output directory
```

The folder contains everything needed to run the program under WebAssembly:

```
dist/app/
  qs_node.js     # Node loader
  qs_node.wasm   # WebAssembly module
  program.qs     # source copy
  program.qvm    # compiled bytecode (faster to start)
```

Run it with **Node.js alone** � no QuantoScript installation required:

```bash
node dist/app/qs_node.js dist/app/program.qs      # from source
node dist/app/qs_node.js run dist/app/program.qvm  # from bytecode (faster)
```

Or, if `qs` is available, through the usual wrapper:

```bash
qs wasm dist/app/program.qs
qs wasm run dist/app/program.qvm
```

### Real-world example

`examples/wasm_demo.qs` is a small program that runs identically on the
interpreter, the VM, native builds, and WebAssembly. Bundling it:

```bash
qs wasm bundle examples/wasm_demo.qs -o wasm_demo_bundle
```

produces:

```
Bundled examples/wasm_demo.qs -> wasm_demo_bundle/
  qs_node.js
  qs_node.wasm
  wasm_demo.qs
  wasm_demo.qvm

Run with Node.js only:
  node wasm_demo_bundle/qs_node.js wasm_demo_bundle/wasm_demo.qs
  node wasm_demo_bundle/qs_node.js run wasm_demo_bundle/wasm_demo.qvm
```

Both invocations print the same output (fibonacci, maps with optional chaining,
higher-order functions, and a caught exception), and the `.qvm` variant starts
noticeably faster. The whole folder is about 530 KB and is the only thing you
need to hand to a user who has Node.js installed.

Any program that uses only the portable core of the language produces the exact
same output as the native binary. See
[`examples/wasm_demo.qs`](../../examples/wasm_demo.qs) for a small program that
runs identically on the interpreter, the VM, native builds, and WebAssembly.

---

## What the WebAssembly target cannot do

The WebAssembly sandbox has no access to the host's operating system beyond the
virtual filesystem, so the platform-dependent features below are unavailable.
Calling them raises a clear runtime error instead of crashing:

| Feature | Builtins affected |
|---------|-------------------|
| Live networking | `http_*`, `websocket_*`, and the HTTP examples |
| Foreign function interface | `ffi_*` |
| POSIX regular expressions | `regex_*` builtins |
| Process spawning | `sys_run`, `sys_capture` |

Everything else — numbers, strings, lists, maps, control flow, functions,
classes, lambdas, imports, comprehensions, exceptions (`try`/`oops`), and the
filesystem — works normally.

---

## Checking parity

`make wasm-test` builds both the native binary and the WebAssembly module, then
runs every test and example through both and compares their standard output,
standard error, and exit code:

```bash
make wasm-test
```

The underlying script is `tests/wasm_compare.ps1` (portable PowerShell Core or
Windows PowerShell). It reports full matches, the files that differ only because
of the platform limitations above (listed in the script as `$platformLimited`),
and any unexpected difference (which fails the run with a non-zero exit code).
This is how the WebAssembly build is kept in sync with the native one.

---

## Next steps

- [Execution Engines](16-execution-engines.md) — the interpreter, VM, and native compiler.
- [Bytecode and Native Builds](17-bytecode-and-native.md) — `.qvm` files and native binaries.

---

[« Previous: Style and Limits](20-style-and-limits.md) · [Guide Index](README.md)
