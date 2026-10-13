[« Previous: Standard Library](14-standard-library.md) · [Guide Index](README.md) · [Next: Execution Engines »](16-execution-engines.md)

# 15. Built-in Functions

These functions are always available — no import required.

---

## Output and input

| Function | Description |
|----------|-------------|
| `print(...)` | print values to standard output |
| `input()` | read a line from standard input |

`print` takes any number of values, separated by spaces. Bare strings print
without quotes, while strings inside a list or map keep their quotes:

```qscript
name = "Quanto"
score = 2 + 3

print(name)                        # Quanto
print("name:", name, "score:", score)   # name: Quanto score: 5
print(["hello"])                   # ['hello']
```

---

## Types and conversion

| Function | Description |
|----------|-------------|
| `type(value)` | the type name, as a string |
| `isinstance(value, name)` | is `value` of the named type? |
| `str(value)` | convert a value to a string |
| `morph(value, type)` | convert a value to another type |
| `len(value)` | length of a string or list |

```qscript
print(type(42))               # int
print(type("hi"))             # string
print(type([1, 2]))           # list
print(isinstance(42, "int"))  # true
print(str(42))                # "42"
print(len("Quanto"))          # 6
print(len([1, 2, 3]))         # 3
```

See [Variables and Types](03-variables-and-types.md) for the full type list.

---

## Converting between types

`morph` changes a value into another type. The target is a type name, written
either as a bare word or as a string. Supported targets are `int`, `float`,
`string`, `bool`, `list`, and `map`.

```qscript
print(morph("5", int))         # 5      (string -> int)
print(morph(5, string))        # 5      (int -> string)
print(morph(10.2, int))        # 10     (float -> int, truncates toward zero)
print(morph("3.14", float))    # 3.14   (string -> float)
print(morph(0, bool))          # false
print(morph("abc", list))      # ['a', 'b', 'c']
print(morph("42", int) + 1)    # 43
```

The type name may also be written as a string, which is handy when it comes from
a variable. Because the names are ordinary values, they work with `isinstance`
too:

```qscript
kind = "int"
print(morph("5", kind))        # 5
print(isinstance(42, int))     # true (same as isinstance(42, "int"))
```

`int` and `integer`, `float`, `double`, and `number`, `str` and `string`, `bool`
and `boolean`, and `map` and `dict` are accepted as aliases. When a value cannot
be read as the requested type — for example `morph("abc", int)` — `morph` raises
an error that you can catch with `try`/`oops`:

```qscript
try {
    morph("abc", int)
} oops e {
    print(e)   # morph cannot read "abc" as int
}
```

See [Variables and Types](03-variables-and-types.md) for the full type list.

---

## Mathematics

| Function | Description |
|----------|-------------|
| `abs(x)` | absolute value |
| `pow(base, exp)` | exponentiation |
| `sqrt(x)` | square root |
| `sin(x)`, `cos(x)`, `tan(x)` | trigonometric functions |
| `log(x)`, `log2(x)`, `log10(x)` | logarithms |
| `ceil(x)` | round up to an integer |
| `floor(x)` | round down to an integer |

```qscript
print(abs(-12))     # 12
print(pow(2, 10))   # 1024
print(sqrt(16))     # 4
print(ceil(1.2))    # 2
print(floor(1.8))   # 1
```

---

## Random

| Function | Description |
|----------|-------------|
| `random()` | random float in `[0, 1)` |
| `random_seed(value)` | seed the generator for reproducibility |

```qscript
random_seed(42)
print(random())
```

For integers, choices, shuffling, and sampling, use
[`stdlib/random.qs`](14-standard-library.md#random).

---

## Ranges and maps

| Function | Description |
|----------|-------------|
| `range(...)` | build a list of numbers |
| `map(...)` | build a map from `key=value` pairs |

```qscript
print(range(5))                    # [1, 2, 3, 4, 5]
print(range(2, 5))                 # [2, 3, 4, 5]
print(range(5, 1, -2))             # [5, 3, 1]

person = map("name" = "Sara", "age" = 20)
print(person)                      # {'name': 'Sara', 'age': 20}
```

---

## Assertions

| Function | Description |
|----------|-------------|
| `assert(condition, message)` | stop if `condition` is false |
| `assert_eq(a, b)` | stop unless `a == b` |
| `assert_ne(a, b)` | stop unless `a != b` |

```qscript
assert(1 + 1 == 2, "math is broken")
assert_eq("a" + "b", "ab")
assert_ne(1, 2)
```

---

## Control

| Function | Description |
|----------|-------------|
| `exit()` | stop the program |
| `ever_return(fn)` | can the function produce a value? |

```qscript
func gives_number() {
    return 2
}

print(ever_return(gives_number))   # true
```

---

## Regular expressions

| Function | Description |
|----------|-------------|
| `regex_match(pattern, text)` | does `text` match `pattern`? |
| `regex_find(pattern, text)` | find the matching part |
| `regex_replace(pattern, text, replacement)` | replace matches |

```qscript
print(regex_match("[0-9]+", "abc123"))   # true
```

---

## Advanced: native interop

These call into C, the low-level WebSocket layer, and Python (when built with
Python support). They are intended for advanced use and library authors.

**Foreign function interface (C libraries)**

| Function | Description |
|----------|-------------|
| `ffi_open(library)` | open a shared library |
| `ffi_symbol(lib, name)` | look up an exported symbol |
| `ffi_call_i(fn)`, `ffi_call_f(fn)`, `ffi_call_s(fn)` | call returning int / float / string |
| `ffi_close(lib)` | close a library |
| `ffi_alloc(size)` | allocate native memory |
| `ffi_free(ptr)` | free native memory |
| `ffi_read_string(ptr)` | read a C string |
| `ffi_write_string(ptr, text)` | write a C string |

**Low-level WebSocket**

`ws_connect`, `ws_send`, `ws_recv`, `ws_close`, `ws_state`, `ws_set_timeout` —
usually used through [`stdlib/websocket.qs`](14-standard-library.md#websocket).

**Tasks**

`spawn`, `task_run`, `task_result`, `task_status`, `task_run_all` — usually used
through [`stdlib/async.qs`](14-standard-library.md#async).

**Python embedding** (requires a build with Python support)

`python_init`, `python_finalize`, `python_eval`, `python_call`, `python_import`,
`python_get`.

```qscript
lib = ffi_open("kernel32.dll")
tick = ffi_symbol(lib, "GetTickCount")
print(ffi_call_i(tick))
ffi_close(lib)
```

---

## Next steps

[Execution Engines](16-execution-engines.md) explains how a program is run.

---

[« Previous: Standard Library](14-standard-library.md) · [Guide Index](README.md) · [Next: Execution Engines »](16-execution-engines.md)
