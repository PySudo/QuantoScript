# QuantoScript Language Reference

> **Version:** 1.0.0  
> **Written in:** C99 (single-file amalgamation with dual execution engine)

Complete documentation for the QuantoScript scripting language — a small, readable language with a tree-walk interpreter *and* a bytecode VM.

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Syntax & Keywords](#syntax--keywords)
5. [Variables & Types](#variables--types)
6. [Operators](#operators)
7. [Control Flow](#control-flow)
8. [Functions](#functions)
9. [Lambda Expressions & Closures](#lambda-expressions--closures)
10. [Classes](#classes)
11. [Collections](#collections)
12. [String Methods & f-strings](#string-methods--f-strings)
13. [List Comprehensions](#list-comprehensions)
14. [Built-in Functions](#built-in-functions)
15. [Exception Handling (try / oops)](#exception-handling-try--oops)
16. [Imports & Modules](#imports--modules)
17. [Standard Library](#standard-library)
18. [Async / Task System](#async--task-system)
19. [Execution Engines](#execution-engines)
20. [Bytecode VM & QVM Format](#bytecode-vm--qvm-format)
21. [Native C Compiler](#native-c-compiler)
22. [CLI Reference](#cli-reference)
23. [REPL (Interactive Mode)](#repl-interactive-mode)
24. [Package Manager](#package-manager)
25. [Known Limitations](#known-limitations)
26. [Common Errors & Debugging](#common-errors--debugging)

---

## Overview

**QuantoScript** is a small, readable scripting language with a friendly syntax. It's inspired by Python's clarity but uses braces `{ }` for blocks instead of significant whitespace. Unusually for a small language, it ships with **two interchangeable execution engines** (a tree-walk interpreter and a bytecode VM) and an optional **native C compiler** for compute-heavy code.

### Design Philosophy

| Principle | Meaning |
|-----------|---------|
| **Readability first** | Python-like clarity, but with braces and more English-like keywords (`maybe`, `oops`) |
| **No significant whitespace** | Blocks are delimited with `{ }` — no indentation errors |
| **Dual engine** | Same code runs identically on the interpreter and the bytecode VM |
| **Feature-rich** | Closures, comprehensions, f-strings, default args, `*args`/`**kwargs` |
| **Self-contained** | Single-file C99 build — easy to compile, vendor, or embed |

### QuantoScript at a Glance

```qscript
# This is a comment
print("Hello, World!")

# Fibonacci
func fibonacci(n) {
    if n <= 1 { return n }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
print(fibonacci(10))  # 55

# Lambda with closure
make_adder = fn(base) -> fn(x) -> base + x
add5 = make_adder(5)
print(add5(3))  # 8

# Class
class Person {
    init(name, age=0) {
        self.name = name
        self.age = age
    }
    greet() { print("Hi, I'm " + self.name) }
}
p = Person("Alice", 25)
p.greet()

# f-strings
name = "QS"
print(f"Hello {name}!")

# List comprehension
squares = [x * x for x in range(5)]
print(squares)  # [1, 4, 9, 16, 25]

# *args / **kwargs
func show(first, *rest, **info) {
    print("first:", first)
    print("rest:", rest)
    print("info:", info)
}
show("a", "b", "c", age=10, city="Tehran")
```

---

## Installation

### One-Line Installer (Recommended)

**macOS / Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1 | iex
```

Then open a new terminal and run `qs --help`.

### Installer Options

**macOS / Linux:**
```bash
# Specific version
curl -fsSL .../install.sh | sh -s -- --version 1.0.0
# Custom install prefix
QUANTO_INSTALL="$HOME/.local" curl -fsSL .../install.sh | sh
# Don't modify shell profile
NO_MODIFY_PATH=1 curl -fsSL .../install.sh | sh
# Uninstall
curl -fsSL .../install.sh | sh -s -- --uninstall
```

**Windows:**
```powershell
& ([scriptblock]::Create((irm .../install.ps1))) -Version 1.0.0
& ([scriptblock]::Create((irm .../install.ps1))) -Uninstall
```

### Building from Source

Requires a C compiler (gcc/clang) and OpenSSL.

**macOS:**
```bash
SSL=$(brew --prefix openssl@3)
cc -std=c99 -O2 -Isrc -I"$SSL/include" src/quanto.c -o qs \
   -L"$SSL/lib" -lssl -lcrypto -lpthread -lm
```

**Linux:**
```bash
gcc -std=c99 -O2 -Isrc src/quanto.c -o qs \
    -lssl -lcrypto -lpthread -ldl -lm
```

**Windows (MSYS2 / MinGW-w64):**
```bash
gcc -std=c99 -O2 -Isrc src/quanto.c -o qs.exe \
    -lssl -lcrypto -lws2_32 -lwinhttp -lwininet -lcrypt32
```

**Or with Makefile:**
```bash
make          # build build/qs
make test     # build and run regression suite
make clean    # remove build artifacts
```

---

## Quick Start

```bash
qs examples/full_tour.qs   # run with interpreter
qs vm examples/full_tour.qs # run with bytecode VM
qs                          # enter REPL
```

### Hello World

```qscript
print("Hello, World!")

func greet(name) {
    return "Hello, " + name + "!"
}

print(greet("QuantoScript"))
print(greet("Ali"))
```

```bash
qs hello.qs
# Hello, World!
# Hello, QuantoScript!
# Hello, Ali!
```

---

## Syntax & Keywords

### Basic Syntax

Blocks are delimited by `{ }`. Comments start with `#`. Statements are separated by newlines.

```qscript
# This is a comment
x = 42
print(x)
```

### Keywords

```
and       break     class     continue
else      false     func      if        import
in        init      maybe     null      not
oops      or        print     repeat    return
self      this      true      try       while
from      fn
```

| Keyword | Meaning | Example |
|---------|---------|---------|
| `maybe` | `else if` | `if x > 10 { ... } maybe x > 5 { ... }` |
| `oops` | `raise` / `except` | `oops("error")` / `} oops e { ... }` |
| `self` | `this` (reference to current object) | `self.name = "Alice"` |
| `func` | Function definition | `func add(a, b) { ... }` |
| `fn` | Lambda expression | `fn(x) -> x * 2` |
| `repeat` | Counted loop | `repeat 5 { ... }` |

### File Loading

The file loader merges multi-line constructs until all brace/paren/bracket depths reach zero. Class definitions, function bodies, and control flow blocks become single logical lines. Recommended: one statement per line inside method bodies.

---

## Variables & Types

### Assignment

```qscript
x = 42            # integer
pi = 3.14         # float
name = "hello"    # string
flag = true       # boolean
nothing = null    # null
```

### Type System

| Type | Example | `type()` returns | Description |
|------|---------|-----------------|-------------|
| Integer | `42`, `-7` | `"int"` | 64-bit signed integer |
| Float | `3.14`, `-0.5` | `"float"` | Double-precision floating point |
| String | `"hello"`, `'world'` | `"string"` | UTF-8 string (single or double quotes) |
| Boolean | `true`, `false` | `"bool"` | Logical values |
| Null | `null` | `"null"` | Absence of value |
| List | `[1, 2, 3]` | `"list"` | Ordered collection (1-based indexing) |
| Map | `{"key": "val"}` | `"map"` | Key-value pairs (dictionary) |
| Object | `Person("Alice")` | `"object"` | Class instance |
| Lambda | `fn(x) -> x*2` | `"lambda"` | Anonymous function |

### 1-based Indexing

Lists and strings use **1-based** indexing:

```qscript
items = [10, 20, 30]
print(items[1])   # 10
print(items[2])   # 20
print(items[-1])  # 30 (last element)

s = "hello"
print(s[1])       # h
print(s[-1])      # o
```

### Slicing

Python-like slicing (1-based):

```qscript
items = [10, 20, 30, 40, 50]
print(items[1:3])    # [10, 20]
print(items[:3])     # [10, 20, 30]
print(items[-2:])    # [40, 50]
```

### Escape Sequences

| Sequence | Decoded | Example |
|----------|---------|---------|
| `\n` | Newline | `"hello\nworld"` |
| `\r` | Carriage return | `"a\rb"` |
| `\t` | Tab | `"col1\tcol2"` |
| `\\` | Backslash | `"path\\to\\file"` |
| `\"` | Double quote | `"say \"hi\""` |
| `\'` | Single quote | `"it's"` |
| `\0` | Null byte | `"abc\0def"` |

### Type Annotations & isinstance

Parameters can have type annotations **enforced at runtime**:

```qscript
func greet(name:string) { print("Hello, " + name) }

# Union types
func process(x:int|string) { print(x) }

# Runtime check
print(isinstance(42, "int"))      # true
print(isinstance("hi", "string")) # true
```

---

## Operators

### Arithmetic

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `+` | Addition / string concat | `3 + 4` | `7` |
| `-` | Subtraction | `10 - 3` | `7` |
| `*` | Multiplication | `3 * 4` | `12` |
| `/` | Integer division | `10 / 3` | `3` |
| `%` | Modulo | `10 % 3` | `1` |

### Comparison

`==` `!=` `<` `>` `<=` `>=`

### Logical

| Operator | Description | Notes |
|----------|-------------|-------|
| `and` | Logical AND | |
| `or` | Logical OR | |
| `not` / `!` | Logical NOT | Both forms work everywhere |
| `in` | Membership | `"x" in list` / `"sub" in string` |

> **Note:** `||` and `&&` are not implemented. Use `or` and `and` instead.

### Compound Assignment

```qscript
x = 10
x += 5   # 15
x -= 3   # 12
x *= 2   # 24
x /= 4   # 6
x %= 4   # 2
```

### Operator Precedence (low to high)

`or` → `and` → `==` `!=` `<` `>` `<=` `>=` → `+` `-` → `*` `/` `%` → `not` `!` unary `-`

---

## Control Flow

### if / maybe / else

```qscript
if x > 10 {
    print("big")
} maybe x > 5 {       # "maybe" replaces "elif"
    print("medium")
} else {
    print("small")
}
```

### while

```qscript
i = 0
while i < 5 {
    print(i)
    i = i + 1
}
```

### repeat

Counted loop:

```qscript
# Fixed count
repeat 5 { print("hello") }

# With index variable
repeat i -> 5 { print(i) }   # 1 2 3 4 5

# Over list items
repeat item -> ["a", "b", "c"] { print(item) }
```

### break / continue

```qscript
i = 0
while i < 10 {
    if i == 5 { break }
    if i == 2 { i += 1; continue }
    print(i)
    i += 1
}
# 0 1 3 4
```

---

## Functions

### Definition

```qscript
func add(a, b) {
    return a + b
}

# Type annotations
func add(x:int, y:int) -> int {
    return x + y
}

# Union type parameter
func process(x:string|int) { print(x) }
```

### Default Parameters

```qscript
func say(text:string, ending:string=" world") {
    return text + ending
}
print(say("hello"))          # hello world
print(say("hello", " QS"))   # hello QS
```

### Rest Parameters (*args)

Capture extra positional arguments as a list:

```qscript
func collect(first, *rest) {
    print("first:", first)
    print("rest:", rest)
}
collect("a", "b", "c", "d")
# first: a
# rest: [b, c, d]
```

### Keyword Arguments (**kwargs)

Capture named arguments as a map:

```qscript
func profile(name:string, **info) {
    print("name:", name)
    print("age:", info["age"])
    print("city:", info["city"])
}
profile("Sara", age=20, city="Tehran")
# name: Sara
# age: 20
# city: Tehran
```

### Mixed Parameters

All four types can be combined:

```qscript
func show(name, *items, count:int=10, **extra) {
    print(name, items, count, extra)
}
```

**Rules:**
- Default parameters must come after non-default ones
- `*args` and `**kwargs` cannot have defaults
- `**kwargs` must come last

### ever_return

Built-in function to check if a function can return a value:

```qscript
func gives_number() { return 2 }
func gives_null() {}
func stops() { exit() }

print(ever_return(gives_number))  # true
print(ever_return(gives_null))    # true
print(ever_return(stops))         # false (exit never returns)
```

### Recursion

```qscript
func factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}
print(factorial(5))  # 120
```

---

## Lambda Expressions & Closures

### Basic Syntax

```qscript
double = fn(x) -> x * 2
print(double(5))  # 10

add = fn(a, b) -> a + b
print(add(3, 4))  # 7
```

### Closures (Working!)

Lambdas **capture outer variables** — closures work in v1.0.0:

```qscript
make_adder = fn(base) -> fn(x) -> base + x
add5 = make_adder(5)
add10 = make_adder(10)

print(add5(3))   # 8
print(add10(3))  # 13

# Counter closure
make_counter = fn() -> {
    count = 0
    return fn() -> { count = count + 1; return count }
}
counter = make_counter()
print(counter())  # 1
print(counter())  # 2
print(counter())  # 3
```

### Lambda with Collections

```qscript
doubled = [1, 2, 3].map(fn(x) -> x * 2)         # [2, 4, 6]
evens = [1, 2, 3, 4].filter(fn(x) -> x % 2 == 0) # [2, 4]
```

### Type Annotations in Lambdas

```qscript
double = fn(x:int) -> x * 2
double("hi")  # Error: argument 'x' has wrong type
```

---

## Classes

### Definition & Instantiation

```qscript
class Person {
    init(name, age=0) {
        self.name = name
        self.age = age
    }
    greet() { print("Hi, I'm " + self.name) }
}

p = Person("Alice", 25)
p.greet()  # Hi, I'm Alice
```

### Constructor (init)

The `init` method is the constructor. It can have default parameters:

```qscript
class Point {
    init(x=0, y=0) {
        self.x = x
        self.y = y
    }
}
p = Point(3, 4)
print(p.x)  # 3
```

### Methods & self

```qscript
class Counter {
    init() { self.count = 0 }
    inc()  { self.count += 1 }
    get()  { return self.count }
}
c = Counter()
c.inc(); c.inc()
print(c.get())  # 2
```

### Object Identity (Reference Semantics)

Assigning an object does NOT copy it — both variables reference the same object:

```qscript
a = Person("Bob")
b = a
b.name = "Charlie"
print(a.name)  # Charlie (changed!)
```

### Type Annotations in Methods

```qscript
class Calculator {
    init() { self.result = 0 }
    add(x:int) { self.result += x }
    get_result() -> int { return self.result }
}
```

### Class Limitations

| Feature | Status |
|---------|--------|
| Constructor (init) | ✅ With default params |
| self | ✅ |
| Type annotations | ✅ Enforced |
| Reference semantics | ✅ |
| Inheritance | ❌ No `extends`/`super` |
| Static methods | ❌ All methods require instance |
| Interfaces | ❌ |
| Operator overloading | ❌ |

---

## Collections

### Lists

```qscript
nums = [3, 1, 4, 1, 5]
print(nums[1])   # 3 (1-based)
print(nums[-1])  # 5
print(nums[2:4]) # [1, 4]
```

#### List Methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `len()` | Number of elements | `[1,2,3].len()` | `3` |
| `push(value)` | Add to end | `[1,2].push(3)` | `[1,2,3]` |
| `append(value)` | Add to end (alias) | `[1,2].append(3)` | `[1,2,3]` |
| `pop()` | Remove & return last | `[1,2,3].pop()` | `3` |
| `contains(value)` | Check existence | `[1,2,3].contains(2)` | `true` |
| `index(value)` | Index of first | `[1,2,3].index(2)` | `2` |
| `remove(value)` | Remove first | `[1,2,3].remove(2)` | `[1,3]` |
| `sort()` | Sort | `[3,1,2].sort()` | `[1,2,3]` |
| `extend(list)` | Add multiple | `[1,2].extend([3,4])` | `[1,2,3,4]` |
| `flatten()` | Flatten nested | `[[1,2],[3]].flatten()` | `[1,2,3]` |
| `sum()` | Sum numbers | `[1,2,3].sum()` | `6` |
| `find(fn)` | Find first matching | `[1,2,3].find(fn(x)->x>1)` | `2` |
| `filter(fn)` | Filter | `[1,2,3].filter(fn(x)->x>1)` | `[2,3]` |
| `map(fn)` | Transform each | `[1,2,3].map(fn(x)->x*2)` | `[2,4,6]` |
| `find_type(type)` | Find by type | `[1,"a",2].find_type("string")` | `"a"` |

### Maps (Dictionaries)

```qscript
user = {"name": "QS", "version": 1}
print(user["name"])  # QS
user["version"] = 2
user["author"] = "PySudo"
```

#### Map Methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `keys()` | List of keys | `user.keys()` | `["name", "version"]` |
| `values()` | List of values | `user.values()` | `["QS", 1]` |
| `items()` | List of [key, value] pairs | `user.items()` | `[["name","QS"], ["version",1]]` |
| `has(key)` | Check key existence | `user.has("name")` | `true` |
| `remove(key)` | Remove a key | `user.remove("name")` | — |
| `len()` | Number of entries | `user.len()` | `2` |

---

## String Methods & f-strings

### Built-in String Methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `upper()` | Uppercase | `"hello".upper()` | `"HELLO"` |
| `lower()` | Lowercase | `"HELLO".lower()` | `"hello"` |
| `title()` | Title case | `"hello world".title()` | `"Hello World"` |
| `len()` | Length | `"hello".len()` | `5` |
| `contains(s)` | Substring check | `"hello".contains("el")` | `true` |
| `replace(old, new)` | Replace | `"hi".replace("i", "ello")` | `"hello"` |
| `startsWith(s)` | Prefix check | `"hello".startsWith("he")` | `true` |
| `endsWith(s)` | Suffix check | `"hello".endsWith("lo")` | `true` |
| `split(sep)` | Split to list | `"a,b,c".split(",")` | `["a", "b", "c"]` |

### f-strings (String Interpolation)

QuantoScript supports **f-strings** — embed expressions directly in strings:

```qscript
name = "QS"
version = 1
print(f"Hello {name}! Version {version + 1}")
# Hello QS! Version 2

# With expressions
print(f"2 + 3 = {2 + 3}")
# 2 + 3 = 5

# With type annotations
func greet(name) {
    return f"Hi, I'm {name}"
}
print(greet("Ali"))  # Hi, I'm Ali
```

Syntax: `f"text {expr} more text"` or `f'text {expr} more text'`.

---

## List Comprehensions

List comprehensions provide a concise way to create lists:

```qscript
# Basic: [expr for var in collection]
squares = [x * x for x in range(5)]
print(squares)  # [1, 4, 9, 16, 25]

# With filter: [expr for var in collection if condition]
evens = [x for x in range(10) if x % 2 == 0]
print(evens)    # [2, 4, 6, 8, 10]

# Strings
chars = [c for c in "hello"]
print(chars)    # [h, e, l, l, o]

# Maps (iterates over keys)
keys = [k for k in {"a": 1, "b": 2}]
print(keys)     # [a, b]
```

Syntax: `[expression for variable in collection]` or `[expression for variable in collection if condition]`

Translates to:
```qscript
result = []
repeat var -> collection {
    if condition {
        result.append(expression)
    }
}
```

---

## Built-in Functions

### Type Functions

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `len(x)` | Length of string/list/map | `len("hi")` | `2` |
| `type(x)` | Type name as string | `type(42)` | `"int"` |
| `str(x)` | Convert to string | `str(42)` | `"42"` |
| `isinstance(x, type)` | Runtime type check | `isinstance(42, "int")` | `true` |

### Math Functions

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `abs(x)` | Absolute value | `abs(-5)` | `5` |
| `sqrt(x)` | Square root | `sqrt(9)` | `3` |
| `pow(x, y)` | Power (x^y) | `pow(2, 3)` | `8` |
| `floor(x)` | Floor | `floor(3.7)` | `3` |
| `ceil(x)` | Ceiling | `ceil(3.2)` | `4` |
| `sin(x)` | Sine (radians) | `sin(0)` | `0` |
| `cos(x)` | Cosine (radians) | `cos(0)` | `1` |
| `tan(x)` | Tangent (radians) | `tan(0)` | `0` |
| `log(x)` | Natural log | `log(1)` | `0` |
| `log2(x)` | Log base 2 | `log2(8)` | `3` |
| `log10(x)` | Log base 10 | `log10(100)` | `2` |

### Range & Random Functions

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `range(n)` | List [1, 2, ..., n] (1-based, inclusive) | `range(3)` | `[1, 2, 3]` |
| `range(start, end)` | List [start, ..., end] (inclusive) | `range(3, 7)` | `[3, 4, 5, 6, 7]` |
| `range(start, end, step)` | With custom step | `range(1, 10, 3)` | `[1, 4, 7, 10]` |
| `random()` | Random float 0.0 to 1.0 | `random()` | `0.372...` |

### Special Functions

| Function | Description | Example |
|----------|-------------|---------|
| `ever_return(fn)` | Check if function can return | `ever_return(some_func)` |
| `exit(code=0)` | Exit program | `exit(1)` |

### Assertion Functions

| Function | Description |
|----------|-------------|
| `assert(cond)` | Assert condition is true |
| `assert_eq(a, b)` | Assert equality |
| `assert_ne(a, b)` | Assert inequality |

### I/O Functions

| Function | Description | Example |
|----------|-------------|---------|
| `print(...)` | Print space-separated values | `print("x =", x)` |
| `input(prompt)` | Read user input | `name = input("Name: ")` |

---

## Exception Handling (try / oops)

### Catching Errors

```qscript
try {
    result = 10 / 0
} oops e {
    print("Error: " + e)
}
# Error: division by zero
```

### Throwing Errors

```qscript
func divide(a, b) {
    if b == 0 { oops("division by zero") }
    return a / b
}

try {
    print(divide(10, 0))
} oops e {
    print(e)  # division by zero
}
```

### Error Variable Binding

```qscript
try {
    x = undefined_var
} oops e {
    print("Caught: " + e)
}
# Caught: undefined variable 'undefined_var'
```

### In Functions

```qscript
func safe_call(fn, arg) {
    try { return fn(arg) }
    oops e { return null }
}
```

---

## Imports & Modules

### from ... import

```qscript
# Standard library
from "stdlib/math.qs" import min, max, clamp

# Local file
from "./helpers.qs" import greet

# From installed package
from "packages/owner_repo/main.qs" import something
```

### Import Scoping

When you import a function, internally-called functions from the same module are NOT auto-imported. Import all dependencies:

```qscript
# BROKEN: year() calls localtime() which isn't imported
# from "stdlib/time.qs" import year

# SOLUTION: import together
from "stdlib/time.qs" import now, localtime, year
```

### Import Resolution

1. Relative to the current file's directory
2. `QUANTO_HOME/stdlib/` directory
3. `./stdlib/` directory (relative to executable)

---

## Standard Library

### math.qs

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `min(items)` | Minimum value in list | `min([3,7,1])` | `1` |
| `max(items)` | Maximum value in list | `max([3,7,1])` | `7` |
| `clamp(value, low, high)` | Clamp to range | `clamp(5, 1, 3)` | `3` |

```qscript
from "stdlib/math.qs" import min, max, clamp
```

### text.qs

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `upper(text)` | Uppercase | `upper("hello")` | `"HELLO"` |
| `lower(text)` | Lowercase | `lower("HELLO")` | `"hello"` |
| `trim(text)` | Strip whitespace | `trim("  hi  ")` | `"hi"` |
| `split(text, sep)` | Split | `split("a,b,c", ",")` | `["a","b","c"]` |

### json.qs

| Function | Description |
|----------|-------------|
| `parse(text)` | Parse JSON string to value |
| `stringify(value)` | Convert value to JSON string |

```qscript
from "stdlib/json.qs" import parse, stringify
json_str = stringify({"name": "QS"})
parsed = parse(json_str)
```

### log.qs

| Function | Description |
|----------|-------------|
| `debug(msg)` | [DEBUG] level |
| `info(msg)` | [INFO] level |
| `warn(msg)` | [WARN] level |
| `error(msg)` | [ERROR] level |

```qscript
from "stdlib/log.qs" import info, error
info("Application started")      # [14:30:22] [INFO] Application started
error("Something went wrong")     # [14:30:22] [ERROR] Something went wrong
```

### fs.qs

| Function | Description |
|----------|-------------|
| `exists(path)` | Check if file/dir exists |
| `read(path)` | Read text file |
| `write(path, text)` | Write to file |
| `mkdir(path)` | Create directory |
| `remove(path)` | Delete file or directory |
| `rename(old, new)` | Rename |
| `is_dir(path)` | Check if directory |
| `list_dir(path)` | List directory contents |
| `basename(path)` | File name from path |
| `dirname(path)` | Parent directory |
| `extension(path)` | File extension |
| `join_path(a, b)` | Join two paths |

```qscript
from "stdlib/fs.qs" import exists, read, write, remove
write("temp.txt", "hello world")
print(read("temp.txt"))   # hello world
remove("temp.txt")
```

### os.qs

> **⚠️ SECURITY WARNING:** `run()` and `capture()` execute shell commands directly. Do not pass untrusted input.

| Function | Description |
|----------|-------------|
| `run(command)` | Run command, return exit code |
| `capture(command)` | Run command, capture stdout |
| `cwd()` | Current working directory |
| `chdir(path)` | Change directory |
| `exists(command)` | Check if command in PATH |

```qscript
from "stdlib/os.qs" import run, capture, cwd
rc = run("echo Hello")
text = capture("git --version")
dir = cwd()
```

> **Note:** `os.exists()` checks PATH for executables. Use `fs.exists()` for file existence.

### time.qs

| Function | Description |
|----------|-------------|
| `now()` | Timestamp (seconds since epoch) |
| `localtime()` | Local time as map: year, month, day, hour, min, sec, wday, yday |
| `format(ts, fmt)` | Format with strftime |
| `parse(text, fmt)` | Parse string to timestamp |
| `year()` / `month()` / `day()` / `hour()` / `minute()` / `second()` | Current time components |
| `weekday()` | Day of week (0=Sunday) |
| `yearday()` | Day of year (0-365) |
| `now_str()` | Current time as `YYYY-MM-DD HH:MM:SS` |
| `today_str()` | Current date as `YYYY-MM-DD` |
| `diff_seconds(t1, t2)` | Difference in seconds |
| `add_seconds(ts, n)` | Add seconds |
| `add_minutes(ts, n)` | Add minutes |
| `add_hours(ts, n)` | Add hours |
| `add_days(ts, n)` | Add days |

```qscript
from "stdlib/time.qs" import now, format, add_days, diff_seconds
t = now()
print(format(t, "%Y-%m-%d"))       # 2024-12-19
tomorrow = add_days(t, 1)
print(diff_seconds(tomorrow, t))   # 86400
```

> **Note:** `year()`, `month()`, etc. internally call `localtime()`. Import all needed functions together.

### random.qs

PCG32-based — deterministic, reproducible.

| Function | Description |
|----------|-------------|
| `seed(value)` | Set seed for reproducibility |
| `randint(min, max)` | Random integer in [min, max] |
| `randrange(start, stop)` | Random integer in [start, stop) |
| `choice(items)` | Random element from list |
| `shuffle(items)` | Fisher-Yates shuffle (new list) |
| `sample(items, k)` | Pick k unique random elements |

```qscript
from "stdlib/random.qs" import seed, randint, choice
seed(42)
x = randint(1, 100)
items = ["red", "green", "blue"]
c = choice(items)
```

### core.qs

| Function | Description |
|----------|-------------|
| `env(name)` | Get environment variable |

### net.qs

| Function | Description |
|----------|-------------|
| `send(host, port, message)` | Send TCP message, get response |
| `send_json(host, port, data)` | Send map as JSON |
| `test(host, port)` | Test reachability |

```qscript
from "stdlib/net.qs" import send
response = send("example.com", 80, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")
```

### websocket.qs

| Function | Description |
|----------|-------------|
| `connect(url)` | Connect to WebSocket |
| `try_connect(url)` | Connect without throwing |
| `send(ws, message)` | Send text message |
| `recv(ws)` | Receive message (blocks) |
| `send_json(ws, data)` | Send as JSON |
| `recv_json(ws)` | Receive and parse JSON |
| `is_open(ws)` | Check connection state |
| `state(ws)` | Get state: "open", "closed", "error" |
| `set_timeout(ws, ms)` | Set receive timeout |
| `close(ws)` | Close connection |

```qscript
from "stdlib/websocket.qs" import connect, send, recv, close
ws = connect("ws://echo.websocket.org")
send(ws, "hello")
msg = recv(ws)
close(ws)
```

### async.qs

See [Async / Task System](#async--task-system) section below.

---

## Async / Task System

QuantoScript has a built-in cooperative task system for concurrency.

### Basic Usage

```qscript
from "stdlib/async.qs" import spawn, run, result, run_all

# Create a task
task = spawn(fn() -> { return 42 })
run(task)
print(result(task))  # 42

# Spawn and run in one step
from "stdlib/async.qs" import spawn_and_run
print(spawn_and_run(fn() -> 1 + 2))  # 3
```

### API Reference

| Function | Description |
|----------|-------------|
| `spawn(fn)` | Create a new task from a lambda |
| `run(task_id)` | Run a task by id |
| `result(task_id)` | Get result from a completed task |
| `status(task_id)` | Check task status |
| `run_all()` | Run all ready tasks cooperatively |
| `spawn_and_run(fn)` | Convenience: spawn, run, and get result |

### Limitations

> **⚠️ The async module is experimental.** Single-task `spawn`/`run`/`result` works reliably. Multi-task usage may hang or crash — see [docs/ASYNC_VALIDATION.md](docs/ASYNC_VALIDATION.md).

---

## Execution Engines

QuantoScript has **three execution engines**:

| Engine | Command | Best For |
|--------|---------|----------|
| **Tree-walk interpreter** | `qs file.qs` | Development, debugging, quick scripts |
| **Bytecode VM** | `qs vm file.qs` | Production: 2-3x faster |
| **Native C compiler** | `qs native file.qs` | Arithmetic code: ~10x speedup |

```bash
qs hello.qs         # interpreter
qs vm hello.qs      # VM (faster)
qs native hello.qs  # native C (requires gcc)
```

### VM vs Interpreter

| Feature | Interpreter | VM |
|---------|-------------|-----|
| All language features | ✅ | ✅ |
| Performance | Baseline | 2-3x faster |
| Recommendation | Development | Production |

---

## Bytecode VM & QVM Format

### Compile to Bytecode

```bash
qs build program.qs                 # -> program.qvm
qs build program.qs -o custom.qvm   # custom output
```

### Run Bytecode

```bash
qs vm program.qvm                   # run compiled bytecode
qs vm --dump-bytecode program.qs    # disassemble
qs vm --trace program.qs            # per-instruction trace
```

### QVM Format (Version 4)

```
┌──────────────────────────────────────┐
│ Header (32 bytes)                    │
├──────────────────────────────────────┤
│ Constant Pool (variable size)        │
├──────────────────────────────────────┤
│ Main Program Instructions            │
├──────────────────────────────────────┤
│ Function Table Entries               │
├──────────────────────────────────────┤
│ Function Body Instructions            │
├──────────────────────────────────────┤
│ Source Path (optional)               │
└──────────────────────────────────────┘
```

- Magic: `0x314D5651` ("QVM1")
- Version 4, with CRC32 checksums
- Each instruction is 13 bytes (opcode + arg_i + const_idx + line)

---

## Native C Compiler

For arithmetic-heavy code:

```bash
qs native program.qs        # emit a_native.c
gcc -O2 a_native.c -o program_native
./program_native
```

**Supported:** integer arithmetic, `if`/`else`, `while`, `repeat`, assignment, `print`
**Not supported:** functions, strings, lists, maps, closures, classes, exceptions

---

## CLI Reference

### Running Code

```bash
qs <file>               # tree-walk interpreter
qs run <file>           # interpreter (same as qs)
qs vm <file>            # bytecode VM
qs build <file>         # compile to .qvm
qs native <file>        # emit native C (a_native.c)
qs compile <file>       # compile to native binary
```

### Development Tools

```bash
qs                      # REPL
qs check <file>         # syntax check
qs fmt <file>           # format source code
qs lint <file>          # report common mistakes
qs doc <file>           # generate docs from comments
qs profile <file>       # profile function calls
qs test [dir]           # run *_test.qs files
```

### Package Management

```bash
qs install <github-url> # install package
qs list                 # list installed
qs remove <package>     # remove package
qs init                 # scaffold a project
```

### Utility

```bash
qs home                 # print QUANTO_HOME
qs version              # print version
qs --help               # show help
```

### Flags

| Flag | Description |
|------|-------------|
| `--dump-bytecode` | Print bytecode to stderr |
| `--trace` | Per-instruction VM trace |
| `--sandbox [path]` | Restrict file access to path |

---

## REPL (Interactive Mode)

```bash
$ qs
>>> x = 42
>>> print(x)
42
>>> func add(a, b) { return a + b }
>>> print(add(3, 4))
7
>>> from "stdlib/math.qs" import abs
>>> print(abs(-5))
5
>>> exit
```

### REPL Features

- Multi-line blocks (auto-continues on `{`)
- Functions, classes, control flow
- Import support
- Expression evaluation
- Exit via `exit` or `quit`

---

## Package Manager

### Creating a Project

```bash
qs init
```

Creates: `main.qs`, `test/` directory, `quanto.json` manifest.

### Installing Packages

```bash
qs install https://github.com/owner/repo
qs list
qs remove repo
```

### Using Packages

```qscript
from "packages/owner_repo/main.qs" import something
```

---

## Known Limitations

### v1.0.0 Limitations

| Limitation | Description | Workaround |
|------------|-------------|------------|
| `||` and `&&` | Not implemented as operators | Use `or` and `and` |
| Method chaining on returns | `split(",").count` fails | Use intermediate variable |
| Single-line try/oops | Not supported in interpreter | Use multi-line syntax |
| Async/await multi-task | Can hang/crash | Single task only |
| Inheritance | No `extends`, `super` | Composition |
| Static methods | Not supported | Create module-level functions |
| Interfaces | Not supported | — |
| Generics | No type parameters | — |
| Pattern matching | No `match`/`switch` | Use `if`/`maybe` |
| Ternary operator | No `? :` | Use `if`/`else` |
| Decorators | Not supported | — |
| Multiple return values | Not supported | Return a list/map |
| Enums | Not supported | — |
| Return type enforcement | Parsed but not checked | — |
| Class field type enforcement | Parsed but not checked | — |

---

## Common Errors & Debugging

### Syntax Errors

```
SyntaxError: unterminated string
line 5, column 12
```

### Type Errors

```qscript
func greet(name:string)
greet(42)
# Error: argument 'name' has the wrong type
```

### Debugging Tips

1. **print() debugging:**
   ```qscript
   print("x =", x)
   ```

2. **Check types:**
   ```qscript
   print(type(x))
   ```

3. **Use assertions:**
   ```qscript
   assert_eq(add(2, 3), 5)
   ```

4. **Syntax check:**
   ```bash
   qs check myfile.qs
   ```

5. **VM trace:**
   ```bash
   qs vm --trace myfile.qs
   ```

6. **Dump bytecode:**
   ```bash
   qs vm --dump-bytecode myfile.qs
   ```

### Stack Traces

```
SyntaxError: division by zero
line 15, column 1
stack trace:
  at divide
  at main
```

---

## Implementation Notes

### Architecture

```
src/
├── quanto.c              # Amalgamation entry point
├── parts/
│   ├── platform.inc      # Platform detection
│   ├── types.inc          # Type definitions
│   ├── values.inc         # Value system (17K lines total)
│   ├── parser.inc         # Tree-walk parser (2,366 lines)
│   ├── executor.inc       # Tree-walk interpreter
│   ├── ast.inc / ast_parser.inc / ast_compiler.inc  # AST + bytecode
│   ├── bytecode.inc / vm.inc     # Bytecode VM (41 opcodes)
│   ├── qvm.inc            # QVM binary format I/O
│   ├── native.inc         # System functions (2,133 lines)
│   ├── native_compiler.inc# C code generator
│   ├── calls.inc          # Function call dispatch
│   ├── blocks_imports.inc # Import resolution
│   └── cli.inc            # CLI handler (1,055 lines)
```

### Performance

- VM: 2-3x faster than interpreter
- String interning: eliminates 70-90% of variable name comparisons
- 41 opcodes in the bytecode instruction set

### Testing

- 132+ regression tests
- Cross-platform CI (macOS, Linux, Windows)
- Release audit score: 96/100

---
 
> **Author:** PySudo  
> **License:** MIT  
> **More Info:** [GitHub Repository](https://github.com/PySudo/QuantoScript)
