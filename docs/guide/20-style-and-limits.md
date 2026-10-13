[« Previous: Package Manager](19-package-manager.md) · [Guide Index](README.md)

# 20. Style and Limits

This final chapter collects the conventions that keep QuantoScript code readable
and the things a v1.1.0 release does *not* do yet.

---

## Style conventions

**Use braces for every block.** QuantoScript ignores indentation, so braces carry
the structure:

```qscript
if ready {
    print("go")
}
```

**One statement per line in multi-statement blocks.** The file loader joins
physical lines until braces are balanced, so keeping one statement per line
avoids surprises, especially inside function and method bodies:

```qscript
func area(w, h) {
    result = w * h
    return result
}
```

**Prefer `maybe` and `oops`.** These are the language's words for "else if" and
"catch", and they read more naturally than symbols:

```qscript
if x > 10 {
    print("big")
} maybe x > 5 {
    print("medium")
} else {
    print("small")
}
```

**Write definitions in uppercase.** Names in all caps are constants, so using
that convention signals intent:

```qscript
MAX_RETRIES = 3
```

**Comment with `#`, and reach for `/* */` for longer notes.**

---

## Known limitations

These are tracked limitations of the v1.1.0 release:

- **Closures** do not capture outer variables. An inner `fn` that reads an outer
  local does not behave as a closure. Use a named `func` instead. See
  [Lambdas and Closures](10-lambdas-and-closures.md#closures-and-currying).
- **Class method bodies** must be written one statement per line. See
  [Classes](11-classes.md).
- **Return type annotations** are parsed but not enforced at runtime.
- **`break` and `continue`** should be used inside multi-line loop bodies.
- **Method chaining on a function's return value** may fail. Assign the
  intermediate result to a variable first:

  ```qscript
  parts = text.split(",")
  print(parts[1])
  ```

- **Async is experimental.** `stdlib/async.qs` is reliable for a single task, but
  the queue can hang or crash with several tasks. See
  [Standard Library § async](14-standard-library.md#async).
- **Python embedding** requires a build with Python support (`-DQS_PYTHON`) and
  the Python headers.

---

## Not yet supported

The following features are not implemented in v1.1.0:

- `&&` and `||` (use `and` and `or`)
- Closures with captured variables
- Inheritance (`extends`, `super`) and interfaces
- Generics, `match` / `switch`, ternary `? :`
- String interpolation (`$"..."`)
- List comprehensions, decorators, structs, enums
- Multiple return values
- A module system beyond file-based imports

Some of these are on the roadmap for upcoming releases.

---

## Where to go next

- Run the examples in `examples/` with the interpreter: `qs examples/full_tour.qs`.
- Browse the [Standard Library](14-standard-library.md) and
  [Built-in Functions](15-built-in-functions.md) references.
- Return to [The QuantoScript Guide](README.md) at any time.

---

[« Previous: Package Manager](19-package-manager.md) · [Guide Index](README.md)
