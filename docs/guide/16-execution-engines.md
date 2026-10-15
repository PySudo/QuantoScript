[« Previous: Built-in Functions](15-built-in-functions.md) · [Guide Index](README.md) · [Next: Bytecode and Native Builds »](17-bytecode-and-native.md)

# 16. Execution Engines

QuantoScript ships with more than one way to run a program. The source language
is the same in every case; only the machinery that executes it changes.

---

## The three engines

| Engine | Command | Best for |
|--------|---------|----------|
| Tree-walk interpreter | `qs program.qs` | Development, debugging, quick scripts |
| Bytecode VM | `qs vm program.qs` | Faster runtime performance |
| Native C compiler | `qs compile program.qs` | Compute-heavy numeric code |

```bash
qs program.qs          # interpreter (the default)
qs vm program.qs       # bytecode VM
qs run program.qs      # bytecode VM (alias)
qs compile program.qs  # produce a standalone native binary
```

---

## The tree-walk interpreter

The default engine walks the parsed program directly and evaluates each node.
It starts instantly, produces clear error messages, and serves as the reference
implementation of the language — the behaviour described throughout this guide is
the interpreter's behaviour.

---

## The bytecode VM

The VM compiles the program to a compact instruction stream first, then runs it
on a virtual machine. This trades a little startup time for faster execution on
long-running code.

```bash
qs vm program.qs
```

Useful flags:

```bash
qs vm --dump-bytecode program.qs   # disassemble to standard error
qs vm --trace program.qs           # per-instruction trace
```

See [Bytecode and Native Builds](17-bytecode-and-native.md) for compiling to a
`.qvm` file.

---

## The native compiler

For arithmetic-heavy code, QuantoScript can emit C and compile it to a native
binary for a large speed-up.

```bash
qs native program.qs        # emit a_native.c
qs compile program.qs       # emit and compile to a standalone binary
```

The native path covers the whole language: numbers, strings, lists, maps, control
flow, functions, classes, exceptions, imports, and comprehensions all lower to C
and compile ahead of time — see
[Bytecode and Native Builds](17-bytecode-and-native.md).

---

## Choosing an engine

- **Writing or debugging?** Use the interpreter (`qs file.qs`).
- **Shipping a script and want speed?** Use the VM (`qs vm file.qs`), or compile
  to `.qvm`.
- **Number crunching in a tight loop?** Try the native compiler.

The interpreter is the safest choice when a feature matters: it is the reference
for every example in this guide. Very new or advanced features may not yet behave
identically on the VM and native backends.

---

## Next steps

[Bytecode and Native Builds](17-bytecode-and-native.md) goes deeper into `.qvm`
files and native compilation.

---

[« Previous: Built-in Functions](15-built-in-functions.md) · [Guide Index](README.md) · [Next: Bytecode and Native Builds »](17-bytecode-and-native.md)
