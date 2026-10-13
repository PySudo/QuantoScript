[« Previous: Variables and Types](03-variables-and-types.md) · [Guide Index](README.md) · [Next: Strings »](05-strings.md)

# 4. Operators

Operators combine values into new values. This chapter covers the arithmetic,
comparison, logic, membership, and assignment operators, followed by the rules
for truthiness.

---

## Arithmetic

| Operator | Meaning | Example | Result |
|----------|---------|---------|--------|
| `+` | addition, string join | `2 + 3` | `5` |
| `-` | subtraction | `7 - 2` | `5` |
| `*` | multiplication, string repeat | `3 * 4` | `12` |
| `/` | division | `10 / 2` | `5` |
| `%` | remainder | `10 % 3` | `1` |
| `-x` | negation | `-5` | `-5` |

```qscript
a = 10
b = 3

print(a + b)   # 13
print(a - b)   # 7
print(a * b)   # 30
print(a / b)   # 3  (integer division: both operands are integers)
print(a % b)   # 1

print((2 + 3) * 4)   # 20
print(-a)            # -10
```

### Division

When both operands are integers, `/` performs **integer** division and truncates
toward zero:

```qscript
print(7 / 2)     # 3
print(-7 / 2)    # -3
```

If either operand is a float, the result is a float:

```qscript
print(7.0 / 2)   # 3.5
```

### Working with strings

`+` joins strings and `*` repeats them (in either order):

```qscript
print("ab" + "cd")   # abcd
print("ha" * 3)      # hahaha
print(3 * "yo")      # yoyoyo
```

---

## Comparison

| Operator | Meaning |
|----------|---------|
| `==` | equal |
| `!=` | not equal |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal |
| `>=` | greater than or equal |

```qscript
print(10 == 10)   # true
print(10 != 5)    # true
print(3 < 5)      # true
print(3 >= 5)     # false
```

Comparisons of mixed types that cannot be equal simply return `false`
(for example `10 == "10"` and `true == 1`).

Lists and maps compare by value, recursively:

```qscript
print([1, [2, 3]] == [1, [2, 3]])              # true
print(map("a"=1) == map("a"=1))                # true
```

### Chained comparisons

Comparisons can be chained like in Python, and mean "all of these at once":

```qscript
x = 3
print(0 < x < 5)   # true
print(0 < x < 2)   # false
```

---

## Logic

| Operator | Meaning |
|----------|---------|
| `and` | logical AND |
| `or` | logical OR |
| `not` | logical NOT |

```qscript
age = 7
print(age > 4 and age < 10)    # true
print(age < 4 or age == 7)     # true
print(not false)               # true
```

> `and`, `or`, and `not` produce boolean results. Use the word forms — the `&&`
> and `||` symbols are not part of the language.

---

## Membership

The `in` operator tests whether a value appears inside a string (as a substring)
or a list (as an element):

```qscript
print("a" in "cat")               # true  (substring)
print("red" in ["red", "blue"])   # true  (element)
print(4 in [1, 2, 3])             # false
```

To test whether a key is present in a map, use the `has` method described in
[Maps](07-maps.md).

---

## Assignment operators

Beyond plain `=`, several short forms update a variable in place:

| Operator | Equivalent to |
|----------|---------------|
| `+=` | `x = x + y` |
| `-=` | `x = x - y` |
| `*=` | `x = x * y` |
| `/=` | `x = x / y` |
| `%=` | `x = x % y` |

```qscript
score = 1
score += 4     # 5
score -= 2     # 3
score *= 3     # 9
score /= 3     # 3
score %= 2     # 1
print(score)
```

Short forms also work on strings:

```qscript
greeting = "hello"
greeting += " world"
print(greeting)   # hello world
```

---

## Truthiness

Conditions treat certain values as false and everything else as true. The falsy
values are:

- `null`
- `false`
- `0`
- `""` (the empty string)
- `[]` (the empty list)

Everything else is truthy:

```qscript
print(not null)   # true
print(not false)  # true
print(not 0)      # true
print(not "")     # true
print(not [])     # true

if "hello" {
    print("non-empty strings are truthy")
}
```

---

## Next steps

Strings are the subject of [Strings](05-strings.md).

---

[« Previous: Variables and Types](03-variables-and-types.md) · [Guide Index](README.md) · [Next: Strings »](05-strings.md)
