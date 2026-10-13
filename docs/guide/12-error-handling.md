[« Previous: Classes](11-classes.md) · [Guide Index](README.md) · [Next: Imports »](13-imports.md)

# 12. Error Handling

When something goes wrong — an out-of-range index, a division by zero, an
explicit failure — QuantoScript reports a runtime *error*. You can either let it
stop the program, or catch it with `try` / `oops`.

---

## Catching errors with `try` / `oops`

`try` runs a block. `oops` is the language's word for "catch": if an error
occurs inside `try`, control jumps to the `oops` block instead of stopping the
program.

```qscript
items = ["red", "green", "blue"]

try {
    print("first:", items[1])
    print("bad:", items[10])     # out of range
    print("never reached")
} oops {
    print("something went wrong, but the program is still alive")
}

print("after try/oops")
```

The `oops` block runs only when the `try` block fails:

```qscript
try {
    print("safe work")
} oops {
    print("this does not run")
}
```

---

## Binding the error

Add a name after `oops` to capture a description of the error:

```qscript
try {
    result = 10 / 0
} oops e {
    print("caught: " + e)
}
```

---

## Raising an error with `oops(...)`

Used as a call, `oops` *raises* an error with your message:

```qscript
func safe_div(a, b) {
    if b == 0 {
        oops("division by zero")
    }
    return a / b
}

try {
    print(safe_div(10, 0))
} oops e {
    print("caught: " + e)
}
```

---

## Assertions

`assert(condition, message)` checks that a condition holds and stops the program
with the message when it does not:

```qscript
age = 25
assert(age > 0, "age must be positive")
print("assert passed")     # reached
```

`assert_eq` and `assert_ne` compare two values:

```qscript
assert_eq(1 + 1, 2)
assert_ne(1, 2)
```

These are handy in tests.

---

## A complete example

```qscript
func check_positive(n) {
    if n <= 0 {
        oops("expected a positive number")
    }
    return n
}

try {
    print(check_positive(42))
    print(check_positive(-1))
} oops e {
    print("failed:", e)
}
```

Single-line `try` / `oops` blocks are also accepted:

```qscript
try { print(check_positive(42)) } oops e { print("failed:", e) }
```

---

## Next steps

[Imports](13-imports.md) shows how to reuse code across files.

---

[« Previous: Classes](11-classes.md) · [Guide Index](README.md) · [Next: Imports »](13-imports.md)
