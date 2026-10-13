[« Previous: Execution Engines](16-execution-engines.md) · [Guide Index](README.md) · [Next: Command-Line Interface »](18-command-line.md)

# 17. Bytecode and Native Builds

Beyond running source directly, QuantoScript can compile a program ahead of time
— either to portable bytecode or to a native binary.

---

## Compiling to bytecode (`.qvm`)

`qs build` compiles a `.qs` file to a `.qvm` bytecode file:

```bash
qs build program.qs                 # writes program.qvm
qs build program.qs -o custom.qvm   # custom output path
```

Run the compiled file with the VM:

```bash
qs run program.qvm
qs vm  program.qvm
```

Compiling ahead of time gives faster startup and lets you distribute a program
without shipping its source.

### The `.qvm` format

A `.qvm` file begins with a magic header and a format version, and carries a
CRC32 checksum of the source. The loader validates the header, the version, and
the checksum, and reports a clear error on malformed files.

### Inspecting bytecode

Disassemble a program for debugging:

```bash
qs vm --dump-bytecode program.qs
```

---

## Compiling to a native binary

The native compiler emits C for a subset of the language, which you can build
with any C compiler:

```bash
qs native program.qs                     # emits a_native.c
gcc -O2 a_native.c -o program_native     # build it
./program_native
```

Or let `qs compile` do both steps:

```bash
qs compile program.qs            # writes a.exe (a.out on Unix); pass an output path to change it
```

`qs compile` needs two things: a C compiler (`gcc`) on your `PATH`, and the
QuantoScript runtime sources (`quanto.c` and `src/parts/`). The sources are
looked up next to the installed binary, under `QUANTO_HOME`, and in the current
directory tree; if you keep them somewhere else, point `QS_SOURCE_DIR` at the
directory that contains `quanto.c`.

### What the native compiler supports

- Integer arithmetic
- `if` / `else`
- `while` and `repeat`
- Variable assignment
- `print`

### What it does not support

Functions, strings, lists, maps, closures, classes, and exceptions are outside
the native path. For everything else, run on the interpreter or the bytecode VM.

---

## Next steps

[Command-Line Interface](18-command-line.md) lists every command and flag.

---

[« Previous: Execution Engines](16-execution-engines.md) · [Guide Index](README.md) · [Next: Command-Line Interface »](18-command-line.md)
