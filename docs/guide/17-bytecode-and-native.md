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

The native compiler lowers the full abstract syntax tree to a self-contained C
file that embeds the QuantoScript runtime, so the resulting program runs without
the interpreter or the VM:

```bash
qs native program.qs             # emits a_native.c
qs native program.qs --build     # emits a_native.c and builds program.exe (program on Unix)
./program
```

Without `--build`, `qs native` only writes the C file. With `--build`, it also
invokes `gcc` on that file to produce a binary named after the source
(`program.qs` → `program.exe` on Windows, `program` elsewhere).

You can also build the generated C yourself:

```bash
gcc -O2 -I <quanto-src> a_native.c -o program_native \
    -lssl -lcrypto -lws2_32 -lwinhttp -lwininet -lcrypt32 -lbcrypt -lregex
```

Because `a_native.c` does `#include "quanto.c"`, compile it with the directory
that holds `quanto.c` on the include path (`-I <quanto-src>`); the link line
above is the same set of libraries used to build `qs` itself. `--build` locates
the runtime sources the same way `qs compile` does (next to the binary, under
`QUANTO_HOME`, or via `QS_SOURCE_DIR`).

If you prefer a single command that always produces a binary:

```bash
qs compile program.qs            # writes a.exe (a.out on Unix); pass an output path to change it
```

`qs compile` needs two things: a C compiler (`gcc`) on your `PATH`, and the
QuantoScript runtime sources (`quanto.c` and `src/parts/`). The sources are
looked up next to the installed binary, under `QUANTO_HOME`, and in the current
directory tree; if you keep them somewhere else, point `QS_SOURCE_DIR` at the
directory that contains `quanto.c`.

### What the native compiler supports

The native backend covers the whole language:

- Integer and float arithmetic, string concatenation, comparison and logic
- `if` / `elif` / `else`, `while`, counted and collection `repeat`, `break` / `continue`
- Every variable, list, map, and string operation
- Functions (including default parameters, `*args`, and `**kwargs`), lambdas,
  and recursion
- Classes, fields, methods, and object construction
- `try` / `oops` exception handling
- `print` and f-strings
- Imports

Programs that use default parameters, `*args`/`**kwargs`, or keyword arguments
delegate those calls to the interpreter at run time (the compiler embeds the
source to build the interpreter's function table), so they behave exactly as
they do under `qs` and `qs vm`.

### Notes

- The generated C relies on GCC/Clang-style builtins for exception handling
  (`__builtin_setjmp` / `__builtin_longjmp`), which are stable across
  optimization levels on MinGW.
- `qs native` is not part of the `qs`, `qs vm`, and `qs build` execution paths;
  it is a separate ahead-of-time compilation target.

---

## Next steps

[Command-Line Interface](18-command-line.md) lists every command and flag.

---

[« Previous: Execution Engines](16-execution-engines.md) · [Guide Index](README.md) · [Next: Command-Line Interface »](18-command-line.md)
