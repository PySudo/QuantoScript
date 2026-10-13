[« Previous: Maps](07-maps.md) · [Guide Index](README.md) · [Next: Functions »](09-functions.md)

# 8. Control Flow

Control flow decides which statements run and how often. QuantoScript provides
`if`/`maybe`/`else` for branching and `while` and `repeat` for looping.

---

## Conditions: `if`, `maybe`, `else`

`maybe` is the language's word for "else if":

```qscript
score = 75

if score >= 90 {
    print("grade: A")
} maybe score >= 70 {
    print("grade: B")
} maybe score >= 50 {
    print("grade: C")
} else {
    print("grade: D")
}
```

Conditions use truthiness — see
[Operators § Truthiness](04-operators.md#truthiness):

```qscript
name = ""

if name {
    print("has a name")
} else {
    print("no name yet")
}
```

Chained comparisons read naturally inside conditions:

```qscript
x = 3
if 0 < x < 5 {
    print("x is between 0 and 5")
}
```

---

## `while`

`while` repeats its block for as long as the condition is true:

```qscript
x = 0
while x < 3 {
    x += 1
    print("x:", x)
}
```

---

## `repeat`

`repeat` comes in several shapes.

### Repeat a fixed number of times

```qscript
counter = 0
repeat 3 {
    counter += 1
    print(counter)
}
```

### Repeat with a counter

`repeat item -> number` counts from `1` up to the number:

```qscript
repeat index -> 5 {
    print(index)
}
```

### Repeat over a collection

The same `repeat item -> collection` form walks strings, lists, and maps:

```qscript
repeat letter -> "abc" {
    print(letter)      # a, b, c
}

repeat color -> ["red", "green", "blue"] {
    print(color)
}

person = map("name" = "Sara", "age" = 20)
repeat key -> person {
    print(key, person[key])
}
```

### Repeat with index and value

`repeat i, item -> collection` gives both the index and the element:

```qscript
names = ["Ali", "Sara", "Mina"]
repeat i, name -> names {
    print(i, name)
}
```

Over a map the two variables receive the key and its value:

```qscript
person = map("name" = "Sara", "age" = 20)
repeat key, value -> person {
    print(key, value)
}
```

---

## There is no C-style `for`

QuantoScript has no `for (i = 0; i < n; i += 1)` loop. Reach for `repeat`
(over a `range` or a collection) or `while` instead:

```qscript
repeat i -> range(3) {
    print(i)      # 1, 2, 3
}
```

---

## `break` and `continue`

- `break` leaves the innermost loop immediately.
- `continue` skips the rest of the current iteration.

```qscript
repeat n -> range(1, 10) {
    if n == 4 {
        break
    }
    print("n:", n)
}

repeat n -> range(1, 6) {
    if n == 3 {
        continue
    }
    print("skip:", n)
}
```

> `break` and `continue` work reliably inside multi-line loop bodies. Keep the
> loop body on its own lines when you use them.

---

## `range`

`range` builds a list of numbers, often used with `repeat`:

```qscript
print(range(5))         # [1, 2, 3, 4, 5]
print(range(2, 5))      # [2, 3, 4, 5]
print(range(5, 1, -2))  # [5, 3, 1]
```

---

## Next steps

[Functions](09-functions.md) shows how to package behaviour into reusable
functions.

---

[« Previous: Maps](07-maps.md) · [Guide Index](README.md) · [Next: Functions »](09-functions.md)
