[« Previous: Syntax Basics](02-syntax-basics.md) · [Guide Index](README.md) · [Next: Operators »](04-operators.md)

# 3. Variables and Types

QuantoScript is dynamically typed: a name can hold any value, and its type can
change from one assignment to the next. This chapter introduces the value types
and the tools you use to inspect them.

---

## Assignment

A statement of the form `name = value` binds a name to a value:

```qscript
x       = 42                 # integer
pi      = 3.14               # float
name    = "Quanto"           # string
flag    = true               # boolean
nothing = null               # null
items   = [1, 2, 3]          # list
user    = {"name": "Sara"}   # map
```

Reassignment is allowed for ordinary (lowercase) names:

```qscript
number = 3
print(number)   # 3
number = 4
print(number)   # 4
```

Uppercase names are *definitions* and cannot be reassigned — see
[Syntax Basics](02-syntax-basics.md#definitions).

---

## The value types

| Type | Example | `type()` returns | Notes |
|------|---------|------------------|-------|
| Integer | `42`, `-7` | `"int"` | 64-bit signed integer |
| Float | `3.14`, `-0.5` | `"float"` | Double-precision floating point |
| String | `"hello"`, `'world'` | `"string"` | UTF-8 text |
| Boolean | `true`, `false` | `"bool"` | Logical values |
| Null | `null` | `"null"` | The absence of a value |
| List | `[1, 2, 3]` | `"list"` | Ordered, 1-based indexing |
| Map | `{"key": "val"}` | `"map"` | Key–value pairs |
| Object | `Person("Alice")` | `"object"` | Instance of a class |
| Lambda | `fn(x) -> x * 2` | `"lambda"` | Anonymous function |

---

## Inspecting a value

`type(value)` returns the type name as a string. `isinstance(value, type)`
answers a yes/no question — the type may be written as a bare word (`int`,
`string`, `list`, ...) or as a string:

```qscript
text = "Quanto"
number = 3
flag = true
nothing = null
items = ["red", "green"]
person = {"name": "Sara"}

print(type(text))                 # string
print(type(number))               # int
print(type(3.5))                  # float
print(type(flag))                 # bool
print(type(nothing))              # null
print(type(items))                # list
print(type(person))               # map

print(isinstance(text, "string")) # true
print(isinstance(number, "int"))  # true
print(isinstance(number, int))    # true (bare type name)
print(isinstance(number, "string")) # false
```

`isinstance` is especially useful inside functions that accept more than one
type — see [Functions](09-functions.md#union-types).

To change a value from one type to another, use `morph` — see
[Built-in Functions](15-built-in-functions.md#converting-between-types).

---

## Null

`null` represents "no value". It is its own type and is *falsy* (see
[Operators](04-operators.md#truthiness)):

```qscript
result = null
print(type(result))   # null
print(isinstance(result, "null"))   # true
```

A function that reaches its end without a `return` yields `null`.

---

## Naming conventions

- Lowercase names (`total`, `userName`) are ordinary, reassignable variables.
- Uppercase names (`MAX_SIZE`, `VERSION`) are definitions meant to be constant.
- A leading underscore (`_temp`) is simply another valid start character.

---

## Next steps

[Operators](04-operators.md) covers the arithmetic, comparison, and logic
operators you can apply to these values.

---

[« Previous: Syntax Basics](02-syntax-basics.md) · [Guide Index](README.md) · [Next: Operators »](04-operators.md)
