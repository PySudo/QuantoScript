# The QuantoScript Guide

Welcome. This guide is a complete, self-contained walkthrough of the QuantoScript
language — its syntax, values, methods, standard library, and the tools that ship
with it.

Every chapter links to the one before and the one after it, so you can read the
whole guide from start to finish like a book, or jump straight to the topic you
need.

---

## How to read this guide

- **New here?** Start with [Getting Started](01-getting-started.md) and continue in order.
- **Looking for something specific?** Use the table below.
- **Writing a program?** Keep [Built-in Functions](15-built-in-functions.md) and the
  [Standard Library](14-standard-library.md) nearby.

---

## Contents

| # | Chapter | What you'll learn |
|---|---------|-------------------|
| 1 | [Getting Started](01-getting-started.md) | Install, run a script, use the REPL |
| 2 | [Syntax Basics](02-syntax-basics.md) | Comments, statements, blocks, identifiers |
| 3 | [Variables and Types](03-variables-and-types.md) | Values, `type`, `isinstance`, definitions |
| 4 | [Operators](04-operators.md) | Arithmetic, comparison, logic, assignment |
| 5 | [Strings](05-strings.md) | Text literals, indexing, slices, methods |
| 6 | [Lists](06-lists.md) | Ordered collections and their methods |
| 7 | [Maps](07-maps.md) | Key–value collections and their methods |
| 8 | [Control Flow](08-control-flow.md) | `if`/`maybe`, `while`, `repeat`, `break`, `continue` |
| 9 | [Functions](09-functions.md) | Parameters, defaults, unions, varargs, `ever_return` |
| 10 | [Lambdas and Closures](10-lambdas-and-closures.md) | `fn` expressions, currying, capture |
| 11 | [Classes](11-classes.md) | `class`, `init`, `self`, shared identity |
| 12 | [Error Handling](12-error-handling.md) | `try`/`oops`, `assert` |
| 13 | [Imports](13-imports.md) | `from ... import ...` and modules |
| 14 | [Standard Library](14-standard-library.md) | HTTP, JSON, files, time, random, WebSocket |
| 15 | [Built-in Functions](15-built-in-functions.md) | Everything available without an import |
| 16 | [Execution Engines](16-execution-engines.md) | Interpreter, bytecode VM, native compiler |
| 17 | [Bytecode and Native Builds](17-bytecode-and-native.md) | `.qvm` files and native binaries |
| 18 | [Command-Line Interface](18-command-line.md) | Every `qs` command and flag |
| 19 | [Package Manager](19-package-manager.md) | Installing, listing, and removing packages |
| 20 | [Style and Limits](20-style-and-limits.md) | Conventions, known limitations, unsupported features |

---

## A thirty-second taste

```qscript
func greet(name) {
    return "Hello, " + name + "!"
}

names = ["Ali", "Sara", "Mina"]

repeat i, name -> names {
    print(i, greet(name))
}

user = {"name": "Quanto", "version": 1}
user["released"] = true
print(user.keys())
```

Run it with the interpreter, the bytecode VM, or compile it to native code — see
[Execution Engines](16-execution-engines.md).

---

[Next: Getting Started »](01-getting-started.md)
