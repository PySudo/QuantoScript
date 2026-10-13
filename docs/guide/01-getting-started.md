[« Previous: Home](README.md) · [Guide Index](README.md) · [Next: Syntax Basics »](02-syntax-basics.md)

# 1. Getting Started

This chapter gets QuantoScript running on your machine and shows you how to
execute your first program.

---

## Installing

The quickest route is the one-line installer. It downloads a prebuilt,
self-contained binary (no external dependencies), places it under `~/.quanto`,
and adds it to your `PATH`.

**macOS and Linux**

```bash
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh
```

**Windows (PowerShell)**

```powershell
irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1 | iex
```

Open a new terminal afterwards and confirm the install:

```bash
qs version
```

> Prebuilt binaries are provided for macOS (Apple Silicon), Linux (x86-64 and
> ARM64), and Windows (x86-64). Prefer to build from source? See
> [Building from source](#building-from-source) below.

---

## Running your first program

Create a file called `hello.qs`:

```qscript
print("Hello, World!")

func greet(name) {
    return "Hello, " + name + "!"
}

print(greet("QuantoScript"))
```

Run it:

```bash
qs hello.qs
```

```
Hello, World!
Hello, QuantoScript!
```

You can run the same file on any execution engine with identical results:

```bash
qs hello.qs          # tree-walk interpreter (the default)
qs vm hello.qs       # bytecode VM
```

See [Execution Engines](16-execution-engines.md) for the full picture.

---

## The interactive REPL

Run `qs` with no arguments to open the read–evaluate–print loop:

```
$ qs
>>> x = 42
>>> print(x)
42
>>> func add(a, b) { return a + b }
>>> print(add(3, 4))
7
>>> exit
```

The REPL keeps state between lines and understands multi-line blocks: if a line
ends while a `{` is still open, it keeps reading until the block is closed.
Type `exit` or `quit` to leave.

---

## A quick map of the CLI

```bash
qs <file.qs>          run a script
qs                    open the REPL
qs vm <file.qs>       run on the bytecode VM
qs build <file.qs>    compile to .qvm bytecode
qs check <file.qs>    validate syntax without running
qs help               list every command
```

The complete command reference is in
[Command-Line Interface](18-command-line.md).

---

## Where the runtime keeps its files

QuantoScript resolves standard-library and package imports relative to
`QUANTO_HOME`. Ask where that points:

```bash
qs home
```

Imports can also resolve relative to the running executable or the current
working directory, which is why the examples use paths such as
`from "../stdlib/time.qs" import now`.

---

## Building from source

The language is a single-file C99 amalgamation, so the build is small.

```bash
# Unix-like systems
make            # produces build/qs
make test       # runs the regression suite

# Windows (PowerShell)
.\build.ps1
```

The compiled binary lands in `build/`. Copy it wherever you like, or run
`make install` if the Makefile provides an install target on your platform.

---

## Next steps

You now have a working install. Continue to
[Syntax Basics](02-syntax-basics.md) to learn how a QuantoScript file is
structured, or skim [The QuantoScript Guide](README.md) to find a topic.

---

[« Previous: Home](README.md) · [Guide Index](README.md) · [Next: Syntax Basics »](02-syntax-basics.md)
