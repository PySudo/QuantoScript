[« Previous: Bytecode and Native Builds](17-bytecode-and-native.md) · [Guide Index](README.md) · [Next: Package Manager »](19-package-manager.md)

# 18. Command-Line Interface

The `qs` executable is both a language runner and a small toolchain. This
chapter lists every command it accepts.

---

## Running programs

```bash
qs                          # interactive REPL
qs <file.qs>                # run with the tree-walk interpreter
qs run <file.qs|.qvm>       # run with the bytecode VM
qs vm <file.qs|.qvm>        # run with the bytecode VM (alias)
qs wasm <file.qs>           # run under WebAssembly via Node
qs wasm vm <file.qs|.qvm>   # run under WebAssembly with the bytecode VM
```

`qs wasm` runs your program on the WebAssembly build that ships with QuantoScript.
It finds the installed runtime and drives [Node.js](https://nodejs.org), which it
looks up on `PATH` (override with the `QS_NODE` environment variable). Everything
after `wasm` is forwarded to the WebAssembly runtime, so `qs wasm vm file.qs`,
`qs wasm build file.qs -o out.qvm`, and `qs wasm run out.qvm` all work. See
[WebAssembly](21-webassembly.md) for the platform's limits.

---

## Building and compiling

```bash
qs build <file.qs> [-o out.qvm]   # compile to .qvm bytecode
qs native <file.qs>               # emit native C (a_native.c)
qs compile <file.qs> [output]     # compile to a native binary
```

See [Bytecode and Native Builds](17-bytecode-and-native.md).

---

## Developing

```bash
qs check <file.qs>      # validate syntax without executing
qs fmt <file.qs>        # format source code
qs lint <file.qs>       # report common mistakes
qs doc <file.qs>        # generate documentation from comments
qs profile <file.qs>    # profile function calls
qs test [directory]     # run *_test.qs files
qs lsp                  # run the language server (JSON-RPC over stdio)
```

`qs lsp` speaks the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
over stdin/stdout. Editors (the bundled VS Code extension in `vscode/` does this
automatically) launch it to get live diagnostics from `qs check` and keyword
hover as you type.

---

## Projects and packages

```bash
qs init                 # scaffold a new project
qs install <url>        # install a package
qs list                 # list installed packages
qs remove <package>     # remove a package
qs home                 # print the QUANTO_HOME directory
```

See [Package Manager](19-package-manager.md).

---

## Information

```bash
qs version              # print the version
qs help                 # show usage
```

---

## Global flags

| Flag | Effect |
|------|--------|
| `--dump-bytecode` | print compiled bytecode to standard error |
| `--trace` | print a per-instruction VM trace |
| `--sandbox [path]` | restrict file access to the given path |

```bash
qs vm --dump-bytecode program.qs
qs --sandbox ./data script.qs
```

---

## Next steps

[Package Manager](19-package-manager.md) covers installing and managing
packages.

---

[« Previous: Bytecode and Native Builds](17-bytecode-and-native.md) · [Guide Index](README.md) · [Next: Package Manager »](19-package-manager.md)
