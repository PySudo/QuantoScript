[« Previous: Functions](09-functions.md) · [Guide Index](README.md) · [Next: Classes »](11-classes.md)

# 10. Lambdas and Closures

A *lambda* is an anonymous function — a function you write inline without giving
it a name. Lambdas are written with `fn` and the arrow `->`.

---

## Lambda syntax

```qscript
double = fn(x) -> x * 2
print(double(5))   # 10

add = fn(a, b) -> a + b
print(add(2, 3))   # 5
```

The expression after `->` is the return value. For a multi-statement body, use a
braced block with an explicit `return`:

```qscript
describe = fn(n) -> {
    if n > 0 {
        return "positive"
    }
    return "not positive"
}
```

---

## Type hints and union types

Lambda parameters accept the same annotations as named functions:

```qscript
typed_add = fn(x:int, y:int) -> x + y
print(typed_add(10, 20))   # 30

concat = fn(x:int|string, y:int|string) -> x + y
print(concat("hello ", 2))    # hello 2
```

---

## Lambdas with list methods

The main use of lambdas is to pass a small function to `map`, `filter`, and
`find`:

```qscript
numbers = [1, 2, 3, 4, 5]

print(numbers.map(fn(x) -> x * 2))                     # [2, 4, 6, 8, 10]
print(numbers.filter(fn(x) -> x % 2 == 0))             # [2, 4]
print(numbers.find(fn(x) -> x > 3))                    # 4
```

See [Lists](06-lists.md) for the complete set of collection methods that take a
function.

---

## Closures and currying

> **Partly supported in v2.0.0.** A lambda captures the enclosing local variables
> in the tree-walk interpreter and in native builds, so simple closures work
> there. The bytecode VM does not capture outer locals and reports an unknown
> variable. Nested/curried lambdas (`fn(x) -> fn(y) -> ...`) are not supported on
> any engine.
>
> See [Style and Limits](20-style-and-limits.md#known-limitations).

When you need a reusable callable, define a named `func` instead — see
[Functions](09-functions.md).

---

## Next steps

[Classes](11-classes.md) introduces objects with `class`, `init`, and `self`.

---

[« Previous: Functions](09-functions.md) · [Guide Index](README.md) · [Next: Classes »](11-classes.md)
