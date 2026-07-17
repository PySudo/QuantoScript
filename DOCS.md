# QuantoScript Language Reference

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
9. [Lambda Expressions](#lambda-expressions)
10. [Classes](#classes)
11. [Collections](#collections)
12. [String Methods](#string-methods)
13. [Built-in Functions](#built-in-functions)
14. [Exception Handling (try / oops)](#exception-handling-try--oops)
15. [Imports & Modules](#imports--modules)
16. [Standard Library](#standard-library)
17. [Execution Engines](#execution-engines)
18. [Bytecode VM & QVM Format](#bytecode-vm--qvm-format)
19. [Native C Compiler](#native-c-compiler)
20. [CLI Reference](#cli-reference)
21. [REPL (Interactive Mode)](#repl-interactive-mode)
22. [Package Manager](#package-manager)
23. [Known Limitations](#known-limitations)
24. [Common Errors & Debugging](#common-errors--debugging)

---

## Overview

**QuantoScript** is a small, readable scripting language with a friendly syntax. It's inspired by Python's clarity but uses braces `{ }` for blocks instead of significant whitespace. Unusually for a small language, it ships with **two interchangeable execution engines** (a tree-walk interpreter and a bytecode VM) and an optional **native C compiler** for compute-heavy code.

### Design Philosophy

| Principle | Meaning |
|-----------|---------|
| **Readability first** | Inspired by Python's clarity, but with braces and more English-like keywords |
| **No significant whitespace** | Blocks are delimited with `{ }` — no indentation errors |
| **Dual engine** | Same code runs identically on the interpreter and the bytecode VM |
| **Predictable semantics** | 1-based indexing, shared object identity (reference semantics) |
| **Self-contained** | Single-file C99 build — easy to compile, vendor, or embed |

### QuantoScript at a Glance

```qscript
# This is a comment
print("Hello, World!")

name = "QuantoScript"
print("Welcome to", name)

# Fibonacci
func fibonacci(n) {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
print(fibonacci(10))  # 55

# Lambda
double = fn(x) -> x * 2
print(double(5))      # 10

# Class
class Person {
    init(name) {
        self.name = name
    }
    greet() {
        print("Hi, I'm " + self.name)
    }
}
p = Person("Alice")
p.greet()             # Hi, I'm Alice

# Exception handling
try {
    print(10 / 0)
} oops e {
    print("caught: " + e)
}
```

---

## Installation

### One-Line Installer (Recommended)

The fastest way to get QuantoScript is a prebuilt binary — no compiler required.

**macOS / Linux:**
```bash
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1 | iex
```

Then open a new terminal and run:
```bash
qs --help
```

### Installer Options

**macOS / Linux:**
```bash
# Install a specific version
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh -s -- --version 1.0.0

# Custom install prefix (default: ~/.quanto)
QUANTO_INSTALL="$HOME/.local" curl -fsSL .../install.sh | sh

# Don't modify your shell profile
NO_MODIFY_PATH=1 curl -fsSL .../install.sh | sh

# Uninstall
curl -fsSL https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.sh | sh -s -- --uninstall
```

**Windows:**
```powershell
# Install a specific version
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1))) -Version 1.0.0

# Uninstall
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/PySudo/QuantoScript/main/install.ps1))) -Uninstall
```

### Building from Source

Prerequisites: a C compiler (gcc/clang) and OpenSSL.

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
make test     # build and run the regression suite
make clean    # remove build artifacts
```

---

## Quick Start

```bash
qs examples/full_tour.qs   # run a script with the interpreter
qs vm examples/full_tour.qs # run with the bytecode VM
qs                          # drop into the REPL
```

### Your First Program

Create `hello.qs`:

```qscript
print("Hello, World!")

func greet(name) {
    return "Hello, " + name + "!"
}

print(greet("QuantoScript"))
print(greet("Ali"))
```

Run it:

```bash
qs hello.qs
# Hello, World!
# Hello, QuantoScript!
# Hello, Ali!
```

### Running with Different Engines

```bash
qs hello.qs         # tree-walk interpreter (development)
qs vm hello.qs      # bytecode VM (faster)
qs native hello.qs  # compile to C native (arithmetic code only)
```

---

## Syntax & Keywords

### Basic Syntax

QuantoScript uses braces `{ }` for blocks. Comments start with `#`. Statements are separated by newlines.

```qscript
# This is a comment
x = 42
print(x)

# Multi-line with braces
func add(a, b) {
    return a + b
}
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
| `oops` | `raise` / `except` | `oops("error")` / `} oops e {` |
| `self` | `this` (reference to current object) | `self.name = "Ali"` |
| `func` | Function definition | `func add(a, b) { ... }` |
| `fn` | Lambda expression | `fn(x) -> x * 2` |
| `repeat` | Counted loop | `repeat 5 { ... }` |

### File Loading

The file loader automatically merges multi-line constructs into single logical lines until all brace/parenthesis/bracket depth counters reach zero. This means:

- Class definitions become one logical line
- Function bodies become one logical line
- **Recommended:** Write one statement per line in method bodies

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

Lists and strings use **1-based** indexing (not 0):

```qscript
items = [10, 20, 30]
print(items[1])   # 10
print(items[2])   # 20
print(items[3])   # 30
print(items[-1])  # 30 (last element)

s = "hello"
print(s[1])       # h
print(s[-1])      # o
```

### Slicing

Python-like slicing (but 1-based):

```qscript
items = [10, 20, 30, 40, 50]
print(items[1:3])    # [10, 20]
print(items[:3])     # [10, 20, 30]
print(items[-2:])    # [40, 50]
```

### Escape Sequences

| Sequence | Decoded Value | Example |
|----------|---------------|---------|
| `\n` | Newline | `"hello\nworld"` |
| `\r` | Carriage return | `"a\rb"` |
| `\t` | Tab | `"col1\tcol2"` |
| `\\` | Backslash | `"path\\to\\file"` |
| `\"` | Double quote | `"say \"hi\""` |
| `\'` | Single quote | `"it's"` |
| `\0` | Null byte | `"abc\0def"` |

```qscript
print("hello\nworld")   # outputs two lines
print("tab\there")      # tab between words
print("path\\to\\file") # path\to\file
```

Invalid escape sequences (e.g., `\q`) are preserved as literal characters.

### Null

```qscript
x = null
if x == null {
    print("x is null")
}
```

---

## Operators

### Arithmetic

| Operator | Description | Example | Result |
|----------|-------------|---------|--------|
| `+` | Addition / string concatenation | `3 + 4` | `7` |
| `-` | Subtraction | `10 - 3` | `7` |
| `*` | Multiplication | `3 * 4` | `12` |
| `/` | Integer division | `10 / 3` | `3` |
| `%` | Modulo (remainder) | `10 % 3` | `1` |

### Comparison

| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |

### Logical

| Operator | Description | Notes |
|----------|-------------|-------|
| `and` | Logical AND | Works everywhere |
| `or` | Logical OR | Works everywhere |
| `not` | Logical NOT | Works everywhere |
| `!` | Logical NOT (alias) | Works everywhere |

> **Note:** `||` and `&&` are **not implemented**. Use `or` and `and` instead.

### Compound Assignment

```qscript
x = 10
x += 5   # x = 15
x -= 3   # x = 12
x *= 2   # x = 24
x /= 4   # x = 6
x %= 4   # x = 2
```

### Operator Precedence (Low to High)

| Level | Operators |
|-------|-----------|
| 1 (lowest) | `or` |
| 2 | `and` |
| 3 | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| 4 | `+`, `-` |
| 5 | `*`, `/`, `%` |
| 6 (highest) | `not`, `!`, unary `-` |

---

## Control Flow

### if / maybe / else

`maybe` is QuantoScript's `elif`:

```qscript
if x > 10 {
    print("big")
} maybe x > 5 {       # "maybe" = "else if"
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
# 0 1 2 3 4
```

### repeat

Counted loop — similar to `for` in other languages:

```qscript
# Fixed count
repeat 5 {
    print("hello")
}

# With index variable
repeat i -> 5 {
    print(i)
}
# 1 2 3 4 5

# Over list items
items = ["a", "b", "c"]
repeat item -> items {
    print(item)
}
# a b c
```

### break / continue

```qscript
i = 0
while i < 10 {
    if i == 5 {
        break
    }
    if i == 2 {
        i = i + 1
        continue
    }
    print(i)
    i = i + 1
}
# 0 1 3 4
```

> **Note:** `break` and `continue` inside single-line blocks may fail due to file loader line merging. Use multi-line syntax.

### `in` Operator

Check membership in a list or substring in a string:

```qscript
names = ["Ali", "Sara", "Mina"]
if "Sara" in names {
    print("found!")
}

text = "hello world"
if "world" in text {
    print("substring found!")
}
```

---

## Functions

### Definition

```qscript
func add(a, b) {
    return a + b
}

# With type annotations
func add(x:int, y:int) -> int {
    return x + y
}

# Union type
func process(x:string|int) {
    print(x)
}
```

### Parameters

| Feature | Status |
|---------|--------|
| Multiple parameters | ✅ Supported |
| Type annotations (`name:type`) | ✅ Enforced at runtime |
| Union types (`string\|int`) | ✅ Supported |
| Default values | ❌ Not supported |
| Rest parameters (`*args`) | ❌ Not supported |
| Named arguments (`**kwargs`) | ❌ Not supported |
| Return type enforcement | ❌ Parsed but not enforced (metadata only) |

### Type Annotations

Type annotations on function and lambda parameters are **enforced at runtime**. A type mismatch raises an error.

**Supported types:** `int`, `float`, `string`, `bool`, `list`, `map`, `null`, `any`

**Union types** with `|`:

```qscript
func add(x:int, y:int) {
    return x + y
}
add(3, 4)     # OK
add("a", 4)   # Error: argument 'x' has the wrong type

func greet(x:string|int) {
    return str(x)
}
greet(42)      # OK (matches int)
greet("hi")    # OK (matches string)
```

**Untyped parameters** accept any value (no annotation = `any`).

**isinstance()** performs runtime type checking:

```qscript
assert_eq(isinstance(42, "int"), true)
assert_eq(isinstance(42, "string"), false)
```

### Recursion

```qscript
func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
print(factorial(5))  # 120

func fibonacci(n) {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
print(fibonacci(10))  # 55
```

**Note:** Recursive `if` blocks must use multi-line syntax.

### Return Values

```qscript
func get_name() {
    return "Alice"
}

name = get_name()

# Functions without return yield null
func just_print() {
    print("hi")
}
result = just_print()  # result = null
```

---

## Lambda Expressions

### Basic Syntax

```qscript
double = fn(x) -> x * 2
print(double(5))  # 10

add = fn(a, b) -> a + b
print(add(3, 4))  # 7

# With type annotation
double = fn(x:int) -> x * 2
```

### Lambda with Collections

```qscript
# map
doubled = [1, 2, 3].map(fn(x) -> x * 2)
print(doubled)  # [2, 4, 6]

# filter
evens = [1, 2, 3, 4, 5].filter(fn(x) -> x % 2 == 0)
print(evens)  # [2, 4]
```

### Lambda Limitations

| Limitation | Description |
|------------|-------------|
| **No closures** | Lambdas cannot capture outer variables in v1.0.0 |
| **Single expression only** | Lambda body is `-> expr` only, no multi-line body |

---

## Classes

### Definition

```qscript
class Person {
    init(name) {
        self.name = name
    }
    greet() {
        print("Hi, I am " + self.name)
    }
}
```

### Instantiation

```qscript
p = Person("Alice")
p.greet()  # Hi, I am Alice
```

### Constructor (init)

The `init` method is the constructor. Arguments passed to `ClassName(args)` are forwarded to `init`.

```qscript
class Point {
    init(x, y) {
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
    init() {
        self.count = 0
    }
    inc() {
        self.count = self.count + 1
    }
    get() {
        return self.count
    }
}
c = Counter()
c.inc()
c.inc()
print(c.get())  # 2
```

### Object Identity

Objects use **shared identity** (reference semantics):

```qscript
a = Person("Bob")
b = a          # b references the SAME object as a
b.name = "Charlie"
print(a.name)  # Charlie (changed!)
```

### Class Limitations

| Limitation | Description |
|------------|-------------|
| **No inheritance** | No `extends`, `super`, or `parent` |
| **No interfaces** | No abstract methods or contracts |
| **No operator overloading** | Operators cannot be customized for classes |
| **Method bodies** | Must use multi-line syntax |
| **No static methods** | All methods require an instance |

---

## Collections

### Lists

Ordered collections of values.

```qscript
nums = [3, 1, 4, 1, 5]
mixed = ["hello", 42, true, null, [1, 2]]

# Indexing (1-based)
print(nums[1])     # 3
print(nums[-1])    # 5

# Slicing
print(nums[2:4])   # [1, 4]
```

#### List Methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `len()` | Number of elements | `[1,2,3].len()` | `3` |
| `push(value)` | Add to end | `[1,2].push(3)` | `[1,2,3]` |
| `append(value)` | Add to end (alias for push) | `[1,2].append(3)` | `[1,2,3]` |
| `pop()` | Remove and return last element | `[1,2,3].pop()` | `3` |
| `contains(value)` | Check if value exists | `[1,2,3].contains(2)` | `true` |
| `index(value)` | Index of first occurrence | `[1,2,3].index(2)` | `2` |
| `remove(value)` | Remove first occurrence | `[1,2,3].remove(2)` | `[1,3]` |
| `sort()` | Sort in place | `[3,1,2].sort()` | `[1,2,3]` |
| `extend(list)` | Add all elements from another list | `[1,2].extend([3,4])` | `[1,2,3,4]` |
| `flatten()` | Flatten nested lists | `[[1,2],[3]].flatten()` | `[1,2,3]` |
| `sum()` | Sum of numeric elements | `[1,2,3].sum()` | `6` |
| `find(fn)` | Find first element matching predicate | `[1,2,3].find(fn(x)->x>1)` | `2` |
| `filter(fn)` | Filter elements with predicate | `[1,2,3].filter(fn(x)->x>1)` | `[2,3]` |
| `map(fn)` | Transform each element | `[1,2,3].map(fn(x)->x*2)` | `[2,4,6]` |
| `find_type(type)` | Find first element of given type | `[1,"a",2].find_type("string")` | `"a"` |

### Maps (Dictionaries)

Key-value pairs.

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
| `has(key)` | Check if key exists | `user.has("name")` | `true` |
| `remove(key)` | Remove a key | `user.remove("name")` | — |
| `len()` | Number of entries | `user.len()` | `3` |

---

## String Methods

### Built-in String Methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `upper()` | Convert to uppercase | `"hello".upper()` | `"HELLO"` |
| `lower()` | Convert to lowercase | `"HELLO".lower()` | `"hello"` |
| `title()` | Capitalize first letter of each word | `"hello world".title()` | `"Hello World"` |
| `len()` | String length | `"hello".len()` | `5` |
| `contains(s)` | Check substring | `"hello".contains("el")` | `true` |
| `replace(old, new)` | Replace substring | `"hi".replace("i", "ello")` | `"hello"` |
| `startsWith(s)` | Check start of string | `"hello".startsWith("he")` | `true` |
| `endsWith(s)` | Check end of string | `"hello".endsWith("lo")` | `true` |
| `split(sep)` | Split into list | `"a,b,c".split(",")` | `["a", "b", "c"]` |

### Examples

```qscript
s = "hello world"
print(s.contains("world"))        # true
print(s.replace("world", "QS"))   # hello QS
print(s.startsWith("hello"))      # true
print(s.endsWith("world"))        # true
print(s.title())                  # Hello World
print(s.upper())                  # HELLO WORLD
print("a,b,c".split(","))         # ["a", "b", "c"]
print(len(s))                     # 11
print(s[1])                       # h
print(s[-1])                      # d
```

---

## Built-in Functions

### Type Functions

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `len(x)` | Length of string, list, or map | `len("hi")` | `2` |
| `type(x)` | Type name as string | `type(42)` | `"int"` |
| `str(x)` | Convert to string | `str(42)` | `"42"` |
| `isinstance(x, type)` | Check runtime type | `isinstance(42, "int")` | `true` |

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
| `log(x)` | Natural logarithm | `log(1)` | `0` |
| `log2(x)` | Log base 2 | `log2(8)` | `3` |
| `log10(x)` | Log base 10 | `log10(100)` | `2` |

### List / Range Functions

| Function | Description |
|----------|-------------|
| `range(n)` | Generate list [1, 2, ..., n] (1-based) |
| `range(start, end)` | Generate list [start, start+1, ..., end] (inclusive) |
| `range(start, end, step)` | Generate list with custom step |
| `random()` | Random float 0.0 to 1.0 |

### Assertion Functions

| Function | Description |
|----------|-------------|
| `assert(cond)` | Assert condition is true (raises error if false) |
| `assert_eq(a, b)` | Assert equality (raises error if not equal) |
| `assert_ne(a, b)` | Assert inequality (raises error if equal) |

### I/O Functions

| Function | Description | Example |
|----------|-------------|---------|
| `print(...)` | Print values separated by spaces | `print("x =", x)` |
| `input(prompt)` | Read user input | `name = input("Enter name: ")` |
| `exit()` | Exit program immediately | `exit()` |
| `exit(code)` | Exit with status code | `exit(1)` |

---

## Exception Handling (try / oops)

### Catching Errors

Use `try / oops` to catch errors:

```qscript
try {
    result = 10 / 0
} oops e {
    print("Error: " + e)
}
# Error: division by zero
```

### Throwing Errors

Use `oops()` to throw an error:

```qscript
func divide(a, b) {
    if b == 0 {
        oops("division by zero")
    }
    return a / b
}

try {
    print(divide(10, 0))
} oops e {
    print(e)  # division by zero
}
```

### Error Variable Binding

The variable after `oops` receives the error message:

```qscript
try {
    x = undefined_var
} oops e {
    print("Caught: " + e)
}
# Caught: undefined variable 'undefined_var'
```

### try/oops in Functions

```qscript
func safe_call(fn, arg) {
    try {
        return fn(arg)
    } oops e {
        return null
    }
}

result = safe_call(fn(x) -> 10 / x, 0)
print(result)  # null
```

---

## Imports & Modules

### from ... import

```qscript
# From the standard library
from "stdlib/math.qs" import min, max, clamp

# From a local file
from "./helpers.qs" import greet

# From an installed package
from "packages/owner_repo/main.qs" import something
```

### Import Scoping

When you import individual functions, internally-called functions from the same module are **not** in scope:

```qscript
# WORKS - leaf function (no internal dependencies):
from "stdlib/time.qs" import now

# BROKEN - year() internally calls localtime() which is not imported:
# from "stdlib/time.qs" import year

# SOLUTION: import all needed functions together:
from "stdlib/time.qs" import now, localtime, year
```

### Import Resolution

The import system searches for modules in this order:

1. **Relative to the current file's directory**
2. **`QUANTO_HOME/stdlib/`** directory
3. **`./stdlib/`** directory (relative to executable)

---

## Standard Library

QuantoScript ships with a standard library accessible via `from "stdlib/<module>.qs" import <names>`.

### math.qs

Basic mathematical operations on lists and numbers.

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `min(items)` | Minimum value in list | `min([3,7,1])` | `1` |
| `max(items)` | Maximum value in list | `max([3,7,1])` | `7` |
| `clamp(value, low, high)` | Clamp value to range | `clamp(5, 1, 3)` | `3` |

```qscript
from "stdlib/math.qs" import min, max, clamp
print(min([3, 7, 1]))  # 1
print(max([3, 7, 1]))  # 7
print(clamp(10, 1, 5)) # 5
```

### text.qs

Additional string operations.

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `upper(text)` | Convert to uppercase | `upper("hello")` | `"HELLO"` |
| `lower(text)` | Convert to lowercase | `lower("HELLO")` | `"hello"` |
| `trim(text)` | Strip leading/trailing whitespace | `trim("  hi  ")` | `"hi"` |
| `split(text, sep)` | Split by separator | `split("a,b,c", ",")` | `["a","b","c"]` |

```qscript
from "stdlib/text.qs" import upper, lower, trim, split
print(upper("hello"))         # HELLO
print(trim("  hi  "))         # hi
```

### json.qs

JSON conversion.

| Function | Description | Example |
|----------|-------------|---------|
| `parse(text)` | Parse JSON string to value | `parse('{"a":1}')` |
| `stringify(value)` | Convert value to JSON string | `stringify({"a":1})` |

```qscript
from "stdlib/json.qs" import parse, stringify
data = {"name": "QS", "version": 1}
json_str = stringify(data)
print(json_str)              # {"name":"QS","version":1}
parsed = parse(json_str)
print(parsed["name"])        # QS
```

### log.qs

Structured logging with timestamps.

| Function | Description |
|----------|-------------|
| `debug(msg)` | Log with [DEBUG] level |
| `info(msg)` | Log with [INFO] level |
| `warn(msg)` | Log with [WARN] level |
| `error(msg)` | Log with [ERROR] level |

```qscript
from "stdlib/log.qs" import info, warn, error
info("Application started")
warn("Low memory")
error("Something went wrong")
# [14:30:22] [INFO] Application started
# [14:30:22] [WARN] Low memory
# [14:30:22] [ERROR] Something went wrong
```

### fs.qs

File system operations.

| Function | Description | Example |
|----------|-------------|---------|
| `exists(path)` | Check if file/directory exists | `exists("file.txt")` |
| `read(path)` | Read text file | `content = read("file.txt")` |
| `write(path, text)` | Write to file | `write("file.txt", "hello")` |
| `mkdir(path)` | Create directory | `mkdir("mydir")` |
| `remove(path)` | Delete file or directory | `remove("file.txt")` |
| `rename(old, new)` | Rename file | `rename("a.txt", "b.txt")` |
| `is_dir(path)` | Check if path is a directory | `is_dir("mydir")` |
| `list_dir(path)` | List directory contents | `list_dir(".")` |
| `basename(path)` | File name from path | `basename("a/b.txt")` → `"b.txt"` |
| `dirname(path)` | Parent directory | `dirname("a/b.txt")` → `"a"` |
| `extension(path)` | File extension | `extension("file.txt")` → `"txt"` |
| `join_path(a, b)` | Join two path components | `join_path("a", "b")` → `"a/b"` |

```qscript
from "stdlib/fs.qs" import exists, read, write, remove, list_dir
write("temp.txt", "hello world")
content = read("temp.txt")
print(content)               # hello world
print(exists("temp.txt"))    # true
files = list_dir(".")
print(files)
remove("temp.txt")
```

### os.qs

Operating system commands.

| Function | Description | Example |
|----------|-------------|---------|
| `run(command)` | Run command and return exit code | `run("echo Hello")` |
| `capture(command)` | Run command and capture stdout | `version = capture("git --version")` |
| `cwd()` | Get current working directory | `dir = cwd()` |
| `chdir(path)` | Change working directory | `chdir("/tmp")` |
| `exists(command)` | Check if command exists in PATH | `exists("gcc")` |

> **⚠️ SECURITY WARNING:** `run()` and `capture()` execute shell commands directly. Do not pass untrusted user input without validation.

```qscript
from "stdlib/os.qs" import run, capture, cwd
rc = run("echo Hello")               # Hello
text = capture("git --version")       # git version 2.x.x
dir = cwd()                           # /home/user/project
```

> **Note:** `os.exists()` checks if a command is available in PATH, not if a file exists. Use `fs.exists()` for file existence checking.

### time.qs

Time and date operations.

| Function | Description | Example |
|----------|-------------|---------|
| `now()` | Current timestamp (seconds since epoch) | `now()` |
| `localtime()` | Local time as a map | `localtime()["year"]` |
| `format(ts, fmt)` | Format timestamp with strftime | `format(now(), "%Y-%m-%d")` |
| `parse(text, fmt)` | Parse string to timestamp | `parse("2024-01-15", "%Y-%m-%d")` |
| `year()` | Current year | `year()` |
| `month()` | Current month (1-12) | `month()` |
| `day()` | Current day (1-31) | `day()` |
| `hour()` | Current hour (0-23) | `hour()` |
| `minute()` | Current minute (0-59) | `minute()` |
| `second()` | Current second (0-59) | `second()` |
| `weekday()` | Day of week (0=Sunday, 6=Saturday) | `weekday()` |
| `yearday()` | Day of year (0-365) | `yearday()` |
| `now_str()` | Current time as `YYYY-MM-DD HH:MM:SS` | `now_str()` |
| `today_str()` | Current date as `YYYY-MM-DD` | `today_str()` |
| `diff_seconds(t1, t2)` | Difference between timestamps in seconds | `diff_seconds(t1, t2)` |
| `add_seconds(ts, n)` | Add seconds | `add_seconds(now(), 3600)` |
| `add_minutes(ts, n)` | Add minutes | `add_minutes(now(), 60)` |
| `add_hours(ts, n)` | Add hours | `add_hours(now(), 24)` |
| `add_days(ts, n)` | Add days | `add_days(now(), 7)` |

```qscript
from "stdlib/time.qs" import now, format, add_days, diff_seconds
t = now()
print(t)                          # 1734567890
print(format(t, "%Y-%m-%d"))      # 2024-12-19
tomorrow = add_days(t, 1)
print(diff_seconds(tomorrow, t))  # 86400
```

> **Note:** `year()`, `month()`, `day()`, `hour()`, `minute()`, `second()`, `weekday()`, `yearday()`, `today_str()`, and `now_str()` internally call `localtime()`. Import `localtime` alongside them.

### random.qs

PCG32-based random number generation — deterministic, portable, reproducible.

| Function | Description | Example |
|----------|-------------|---------|
| `seed(value)` | Set the random seed | `seed(42)` |
| `randint(min, max)` | Random integer in [min, max] | `randint(1, 10)` |
| `randrange(start, stop)` | Random integer in [start, stop) | `randrange(0, 10)` |
| `choice(items)` | Random element from list | `choice([10,20,30])` |
| `shuffle(items)` | Fisher-Yates shuffle (returns new list) | `shuffle([1,2,3])` |
| `sample(items, k)` | Pick k unique random elements | `sample([1..10], 3)` |

```qscript
from "stdlib/random.qs" import seed, randint, choice, shuffle
seed(42)
x = randint(1, 100)
print(x)                         # 77 (deterministic with seed 42)
items = ["red", "green", "blue"]
c = choice(items)
shuffled = shuffle([1, 2, 3, 4, 5])
```

### core.qs

Environment variable access.

| Function | Description |
|----------|-------------|
| `env(name)` | Get environment variable value |

```qscript
from "stdlib/core.qs" import env
path = env("PATH")
home = env("HOME")
print(home)  # /home/username
```

### net.qs

Low-level TCP networking.

| Function | Description | Example |
|----------|-------------|---------|
| `send(host, port, message)` | Send message and receive response | `send("example.com", 80, "GET /")` |
| `send_json(host, port, data)` | Send map as JSON and receive response | `send_json("api.example.com", 8080, {"key": "val"})` |
| `test(host, port)` | Test if host:port is reachable | `test("localhost", 80)` |

```qscript
from "stdlib/net.qs" import send, send_json, test

# Raw TCP
response = send("example.com", 80, "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")

# JSON
data = {"action": "ping"}
response = send_json("localhost", 3000, data)
```

### websocket.qs

Stateful WebSocket client.

| Function | Description |
|----------|-------------|
| `connect(url)` | Connect to a WebSocket server |
| `try_connect(url)` | Connect without throwing on failure |
| `send(ws, message)` | Send a text message |
| `recv(ws)` | Receive a message (blocks) |
| `send_json(ws, data)` | Send a map as JSON |
| `recv_json(ws)` | Receive and parse JSON |
| `is_open(ws)` | Check if connection is open |
| `state(ws)` | Connection state: `"open"`, `"closed"`, or `"error"` |
| `set_timeout(ws, ms)` | Set receive timeout in milliseconds |
| `close(ws)` | Close the connection gracefully |

```qscript
from "stdlib/websocket.qs" import connect, send, recv, close, send_json, recv_json

ws = connect("ws://echo.websocket.org")
send(ws, "hello")
msg = recv(ws)
print(msg)  # hello

send_json(ws, {"type": "greeting", "text": "hello"})
response = recv_json(ws)
close(ws)
```

### async.qs (Experimental)

Async task management. **This module is experimental** — it may crash with two or more tasks under load.

| Function | Description |
|----------|-------------|
| `spawn(fn)` | Create a new task from a lambda |
| `run(task_id)` | Run a task by id |
| `result(task_id)` | Get result from a completed task |
| `status(task_id)` | Check task status |
| `run_all()` | Run all ready tasks cooperatively |
| `spawn_and_run(fn)` | Convenience: spawn, run, and get result |

Single-task `spawn`/`run`/`result` works; multi-task usage is unreliable.

---

## Execution Engines

QuantoScript has **three execution engines**:

| Engine | Command | Best For |
|--------|---------|----------|
| **Tree-walk interpreter** | `qs file.qs` | Development, debugging, quick scripts |
| **Bytecode VM** | `qs vm file.qs` | Production: 2-3x faster than interpreter |
| **Native C compiler** | `qs native file.qs` | Arithmetic-heavy code: ~10x speedup |

All three engines take the same source code and produce **identical output**.

```bash
qs hello.qs            # interpreter
qs vm hello.qs         # bytecode VM
qs native hello.qs     # native C (requires gcc)
```

### VM vs Interpreter

| Feature | Interpreter | VM |
|---------|-------------|-----|
| Arithmetic | ✅ | ✅ |
| Strings | ✅ | ✅ |
| Lists | ✅ | ✅ |
| Maps | ✅ | ✅ |
| Functions | ✅ | ✅ |
| Classes | ✅ | ✅ |
| Imports | ✅ | ✅ |
| Exceptions | ✅ | ✅ |
| Performance | Baseline | 2-3x faster |

> **Recommendation:** Use `qs file.qs` (interpreter) during development. Use `qs vm file.qs` for production runs.

---

## Bytecode VM & QVM Format

### Compiling to Bytecode

```bash
qs build program.qs                 # -> program.qvm
qs build program.qs -o custom.qvm   # custom output path
```

### Running Bytecode

```bash
qs vm program.qvm                   # run compiled bytecode
qs vm --dump-bytecode program.qs    # disassemble for debugging
qs vm --trace program.qs            # print per-instruction trace
```

### QVM Format (Version 4)

The `.qvm` format is a stable binary format with:

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

| Feature | Status |
|---------|--------|
| Magic header | ✅ `0x314D5651` (ASCII "QVM1") |
| Version validation | ✅ Version 4 |
| CRC32 source checksums | ✅ |
| Malformed file handling | ✅ Comprehensive |

Each instruction is 13 bytes: opcode (1), integer arg (4), const index (4), source line (4).

---

## Native C Compiler

For arithmetic-heavy code, QuantoScript can emit C code and compile it to a native binary:

```bash
qs native program.qs        # emit a_native.c
gcc -O2 a_native.c -o program_native
./program_native
```

### Supported

- Integer arithmetic (`+`, `-`, `*`, `/`, `%`)
- `if` / `else`
- `while` loops
- `repeat` loops
- Variable assignment
- `print`

### Not Supported

- Functions and calls
- Strings, lists, maps
- Closures, lambdas
- Classes, objects
- Exceptions (`try`/`oops`)
- Imports

---

## CLI Reference

### Running Code

```bash
qs <file>               # run with tree-walk interpreter
qs run <file>           # run with interpreter (same as qs)
qs vm <file>            # run with bytecode VM
qs build <file>         # compile to bytecode (.qvm)
qs native <file>        # emit native C (a_native.c)
qs compile <file>       # compile to native binary
```

### Development Tools

```bash
qs                      # interactive REPL
qs check <file>         # syntax check (no execution)
qs fmt <file>           # format source code
qs lint <file>          # report common mistakes
qs doc <file>           # generate documentation from comments
qs profile <file>       # profile function calls
qs test [dir]           # run *_test.qs files
```

### Package Management

```bash
qs install <github-url> # install a package from GitHub
qs list                 # list installed packages
qs remove <package>     # remove a package
qs init                 # scaffold a new project
```

### Utility

```bash
qs home                 # print the QUANTO_HOME directory
qs version              # print the version
qs --help               # show help
```

### Flags

| Flag | Description |
|------|-------------|
| `--dump-bytecode` | Print compiled bytecode to stderr |
| `--trace` | Print per-instruction VM trace |
| `--sandbox [path]` | Restrict file access to the given path |

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Runtime error or syntax error |

---

## REPL (Interactive Mode)

Run `qs` without arguments to enter the interactive REPL:

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

| Feature | Supported |
|---------|-----------|
| Multi-line blocks | ✅ Auto-continues on `{` |
| Function definitions | ✅ `func` |
| Class definitions | ✅ `class` |
| Control flow | ✅ `if`/`maybe`/`else`, `while`, `repeat`, `try`/`oops` |
| Imports | ✅ `from ... import` |
| Expression evaluation | ✅ |
| Exit | ✅ `exit` or `quit` |

---

## Package Manager

QuantoScript has a built-in, git-backed package manager.

### Creating a Project

```bash
qs init
```

This scaffolds a project structure:

```
my-project/
├── main.qs
├── test/
│   └── main_test.qs
└── quanto.json
```

### Installing Packages

```bash
# Install from GitHub
qs install https://github.com/owner/repo

# List installed packages
qs list

# Remove a package
qs remove repo
```

### Using Installed Packages

```qscript
from "packages/owner_repo/main.qs" import something
```

### quanto.json (Manifest)

The `quanto.json` file contains project metadata and dependencies. Dependencies are resolved recursively during installation.

---

## Known Limitations

### Language Limitations (v1.0.0)

| Limitation | Description | Workaround |
|------------|-------------|------------|
| **Closures** | Inner functions cannot capture outer variables | Pass variables as parameters instead |
| **Async/await** | `async.qs` crashes with 2+ tasks | Use single-task spawn/run/result only |
| `\|\|` and `&&` | Not implemented as operators | Use `or` and `and` |
| **Method chaining** | `split(",").count` fails on function return values | Use an intermediate variable |
| **Single-line try/oops** | Tree-walk parser does not support single-line | Use multi-line syntax |
| **Break/continue** | May fail inside single-line blocks | Use multi-line block bodies |
| `not` in function args | `fn(not x)` does not work | Compute the value first |
| **Return type enforcement** | Return annotations parsed but not enforced | Documentation only |
| **Class field type annotations** | Parsed but not enforced | Documentation only |

### Missing Features (Not in v1.0.0)

- Inheritance (`extends`, `super`)
- Interfaces / abstract methods
- Generics / type parameters
- Pattern matching (`match`/`switch`)
- Ternary operator (`? :`)
- `$"..."` string interpolation
- List comprehensions
- Decorators
- Multiple return values
- Enums
- Default parameter values
- Rest parameters (`*args`)
- Named arguments (`**kwargs`)

---

## Common Errors & Debugging

### Syntax Error

If your code has a syntax error, you'll see a message with the file, line, and column:

```
SyntaxError: unterminated string
line 5, column 12
```

### Type Error

Runtime errors from type annotation enforcement:

```qscript
func greet(name:string)
greet(42)
# Error: argument 'name' has the wrong type
```

### Import Error

```
from "nonexistent.qs" import x
# Error: could not load module 'nonexistent.qs'
```

### Debugging Tips

1. **Use `print()` for basic debugging:**
   ```qscript
   x = 42
   print("x =", x)   # x = 42
   ```

2. **Check types with `type()`:**
   ```qscript
   print(type(x))     # int, string, list, ...
   ```

3. **Use `assert_eq()` for testing:**
   ```qscript
   assert_eq(add(2, 3), 5)
   ```

4. **Validate syntax without running:**
   ```bash
   qs check myfile.qs
   ```

5. **Enable VM tracing:**
   ```bash
   qs vm --trace myfile.qs
   ```

6. **Dump bytecode for inspection:**
   ```bash
   qs vm --dump-bytecode myfile.qs
   ```

### Stack Trace

Runtime errors include a full call stack:

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
│   ├── values.inc         # Value system (object model)
│   ├── parser.inc         # Tree-walk parser (2,366 lines)
│   ├── executor.inc       # Tree-walk interpreter
│   ├── ast.inc            # AST node definitions
│   ├── ast_parser.inc     # AST parser (for bytecode compiler)
│   ├── ast_compiler.inc   # AST -> bytecode compiler
│   ├── bytecode.inc       # Bytecode instruction definitions
│   ├── vm.inc             # Bytecode VM
│   ├── qvm.inc            # QVM binary format I/O
│   ├── native.inc         # Native (system) functions (2,133 lines)
│   ├── native_compiler.inc# C code generator
│   ├── cli.inc            # CLI handler (1,055 lines)
│   └── ...
```

### Performance

- VM runs **2-3x faster** than the tree-walk interpreter
- String interning eliminates 70-90% of variable name comparisons
- 41 opcodes in the bytecode instruction set
- Inline storage for up to 8 local variables (eliminates heap allocation)

### Object Model

- Objects use **shared identity** (reference semantics) — no deep copy
- `clone_value()` copies the pointer, not the object
- `c = a` means `c` and `a` point to the same object
- All ObjectData tracked in an `object_registry` for cleanup at exit

### Testing

- 132+ regression tests
- Cross-platform CI (macOS, Linux, Windows)
- Separate regression runner with PowerShell
- Release audit score: 96/100

---

