# QuantoScript Language Reference

Complete documentation for QuantoScript v1.0.0.

---

## Table of Contents

1. [Syntax Overview](#syntax-overview)
2. [Keywords](#keywords)
3. [Variables and Types](#variables-and-types)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Lambda Expressions](#lambda-expressions)
8. [Classes](#classes)
9. [Collections](#collections)
10. [String Methods](#string-methods)
11. [Exception Handling](#exception-handleshooting)
12. [Imports](#imports)
13. [Built-in Functions](#built-in-functions)
14. [Standard Library](#standard-library)
15. [Bytecode VM](#bytecode-vm)
16. [REPL](#repl)
17. [CLI Commands](#cli-commands)
18. [Parser Limitations](#parser-limitations)
19. [Unsupported Features](#unsupported-features)

---

## Syntax Overview

QuantoScript uses braces for block delimiting (not indentation). Comments use `#`. Statements are separated by newlines.

```qscript
# This is a comment
x = 42
print(x)
```

### File Loading

The file loader merges multi-line constructs into single logical lines until all brace/parenthesis/bracket depth counters reach zero. This means:

- Class definitions become one logical line
- Function bodies become one logical line
- One statement per line recommended in method bodies

## Keywords

```
and       break     class     continue
else      false     func      if        import
in        init      maybe     null      not
oops      or        print     repeat    return
self      this      true      try       while
from      fn
```

## Variables and Types

### Assignment

```qscript
x = 42          # integer
pi = 3.14       # float
name = "hello"  # string
flag = true      # boolean
nothing = null   # null
```

### Types

| Type | Example | type() returns | Description |
|------|---------|----------------|-------------|
| Integer | `42`, `-7` | `"int"` | 64-bit signed integer |
| Float | `3.14`, `-0.5` | `"float"` | Double-precision floating point |
| String | `"hello"`, `'world'` | `"string"` | UTF-8 string (single or double quotes) |
| Boolean | `true`, `false` | `"bool"` | Logical values |
| Null | `null` | `"null"` | Absence of value |
| List | `[1, 2, 3]` | `"list"` | Ordered collection (1-based indexing) |
| Map | `{"key": "val"}` | `"map"` | Key-value pairs |
| Object | `Person("Alice")` | `"object"` | Class instance |
| Lambda | `fn(x) -> x*2` | `"lambda"` | Anonymous function |

### 1-based Indexing

Lists and strings use 1-based indexing:

```qscript
items = [10, 20, 30]
print(items[1])   # 10
print(items[2])   # 20

s = "hello"
print(s[1])       # h
```

### Escape Sequences

String literals support standard escape sequences, decoded at parse time:

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
print("hello\nworld")   # outputs:
                         # hello
                         # world

print("tab\there")      # tab	here

print("back\\slash")    # back\slash

print("\\n")            # \n (literal backslash-n)

print("\\")             # \ (single backslash)
```

Invalid escape sequences (e.g., `\q`) are preserved as literal characters:

```qscript
print("\q")             # \q (backslash + q)
```

Both single-quoted and double-quoted strings support escape sequences.

## Operators

### Arithmetic

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition / string concatenation | `3 + 4` -> `7` |
| `-` | Subtraction | `10 - 3` -> `7` |
| `*` | Multiplication | `3 * 4` -> `12` |
| `/` | Division (integer) | `10 / 3` -> `3` |
| `%` | Modulo | `10 % 3` -> `1` |

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

| Operator | Description | Limitations |
|----------|-------------|-------------|
| `and` | Logical AND | Works everywhere |
| `or` | Logical OR | Works everywhere |
| `not` | Logical NOT | Works everywhere |
| `!` | Logical NOT (alias) | Works everywhere |

**Note:** `||` and `&&` are **not implemented** as operators. Use `or` and `and` instead.

### Compound Assignment

| Operator | Description |
|----------|-------------|
| `=` | Assignment |
| `+=` | Add-assign |
| `-=` | Subtract-assign |
| `*=` | Multiply-assign |
| `/=` | Divide-assign |
| `%=` | Modulo-assign |

## Control Flow

### if / maybe / else

```qscript
if x > 10 {
    print("big")
} maybe x > 5 {
    print("medium")
} else {
    print("small")
}
```

`maybe` is QuantoScript's `elif`.

### while

```qscript
i = 0
while i < 10 {
    print(i)
    i = i + 1
}
```

### repeat

```qscript
repeat 5 {
    print("hello")
}
```

### break

```qscript
i = 0
x = 0
while i < 10 {
    if i == 5 {
        break
    }
    x = x + i
    i = i + 1
}
print(x)  # 10
```

**Note:** `break` requires multi-line `if` bodies. Single-line `if { break }` on the same line as the loop body may fail due to file loader line merging.

## Functions

### Definition

```qscript
func add(a, b) {
    return a + b
}

func greet(name) {
    print("Hello, " + name)
}
```

### Parameters

- Functions support multiple parameters
- Parameters can have optional type annotations using `name:type` syntax
- No default values, rest parameters, or named arguments

### Type Annotations

Type annotations on function and lambda parameters are **enforced at runtime**. A type mismatch raises an error.

**Supported types:** `int`, `float`, `string`, `bool`, `list`, `map`, `null`, `any`

**Union types** are supported with `|`:

```qscript
func add(x:int, y:int) {
    return x + y
}
add(3, 4)     # OK
add("a", 4)   # Error: argument 'x' has the wrong type

func greet(x:string|int) {
    return str(x)
}
greet(42)      # OK (42 matches int)
greet("hi")    # OK (matches string)
```

**Lambda type annotations** work the same way:

```qscript
double = fn(x:int) -> x * 2
double(5)     # OK
double("hi")  # Error: argument 'x' has wrong type
```

**Untyped parameters** accept any value (no annotation = `any`):

```qscript
func identity(x) {
    return x
}
identity(42)      # OK
identity("hi")    # OK
```

**Return type annotations** are accepted by the parser but **not enforced** at runtime. They serve as documentation only.

**Class method type annotations** are enforced:

```qscript
class Counter {
    init() {
        self.count = 0
    }
    inc(x:int) {
        self.count = self.count + x
    }
}
c = Counter()
c.inc(5)      # OK
c.inc("bad")  # Error: argument 'x' has the wrong type
```

**`isinstance()`** performs runtime type checking using the same type engine:

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
```

**Note:** Recursive `if` blocks must use multi-line syntax.

### Return Values

```qscript
func get_name() {
    return "Alice"
}

name = get_name()
```

## Lambda Expressions

```qscript
double = fn(x) -> x * 2
print(double(5))  # 10

add = fn(a, b) -> a + b
print(add(3, 4))  # 7
```

**Note:** Closures with captured variables do not work in v1.0.0.

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

    get_name() {
        return self.name
    }
}
```

### Instantiation

```qscript
p = Person("Alice")
p.greet()  # Hi, I am Alice
```

### Constructors

The `init` method is the constructor. It receives arguments passed during instantiation.

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

### Methods

Methods have access to `self`:

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

Objects use shared identity (reference semantics):

```qscript
a = Person("Bob")
b = a
b.greet()  # Same object as a
```

### Class Limitations

- **No inheritance** -- no `extends`, `super`, or `parent`
- **No interfaces** -- no abstract methods or contracts
- **No operator overloading**
- **Method bodies** -- must use multi-line syntax (one statement per line inside `{ }`)
- **No static methods** -- all methods require an instance

## Collections

### Lists

```qscript
nums = [3, 1, 4, 1, 5]
print(nums[1])           # 3 (1-based)
print(len(nums))         # 5

nums.extend([6, 7])      # add elements
print(nums.contains(4))  # true
print(nums.sum())        # 21
```

**List methods:** `contains`, `sort`, `extend`, `flatten`, `sum`, `find`, `filter`, `map`, `find_type`

### Maps

```qscript
user = {"name": "QS", "version": 1}
print(user["name"])      # QS
print(user.keys())       # [name, version]
print(user.has("name"))  # true
user.remove("name")
```

**Map methods:** `keys`, `values`, `has`, `remove`

## String Methods

```qscript
s = "hello world"
print(s.contains("world"))     # true
print(s.replace("world", "QS"))  # hello QS
print(s.startsWith("hello"))   # true
print(s.endsWith("world"))     # true
print(s.title())               # Hello World
```

### Via stdlib imports:

```qscript
from "stdlib/text.qs" import upper, lower, trim, split
print(upper("hello"))           # HELLO
print(lower("HELLO"))           # hello
print(trim("  hi  "))           # hi
parts = split("a,b,c", ",")
print(len(parts))               # 3
```

## Exception Handling

### try / oops

```qscript
try {
    result = 10 / 0
} oops e {
    print("Error: " + e)
}
```

### Throwing Errors

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
```

## Imports

### from ... import

```qscript
from "stdlib/math.qs" import min, max, clamp
print(min([3, 7, 1]))  # 1
print(max([3, 7, 1]))  # 7
print(clamp(5, 1, 3))  # 3
```

### Import Scoping

When you import individual functions, internally-called functions from the same module are **not** in scope. Import all functions that depend on each other:

```qscript
# WORKS - leaf functions (no internal dependencies):
from "stdlib/time.qs" import now, add_seconds, diff_seconds

# BROKEN - functions that call other module functions internally:
# from "stdlib/time.qs" import year  # year() calls localtime() which is not imported
```

### Import Resolution

The import system searches for modules in this order:

1. Relative to the current file's directory
2. `QUANTO_HOME/stdlib/` directory
3. `./stdlib/` directory (relative to executable)

## Built-in Functions

### Type Functions

| Function | Description | Example |
|----------|-------------|---------|
| `len(x)` | Length of string, list, or map | `len("hi")` -> `2` |
| `type(x)` | Type name as string | `type(42)` -> `"int"` |
| `str(x)` | Convert to string | `str(42)` -> `"42"` |

### Math Functions

| Function | Description | Example |
|----------|-------------|---------|
| `abs(x)` | Absolute value | `abs(-5)` -> `5` |
| `sqrt(x)` | Square root | `sqrt(9)` -> `3` |
| `pow(x, y)` | Power | `pow(2, 3)` -> `8` |
| `floor(x)` | Floor | `floor(3.7)` -> `3` |
| `ceil(x)` | Ceiling | `ceil(3.2)` -> `4` |
| `sin(x)` | Sine (radians) | `sin(0)` -> `0` |
| `cos(x)` | Cosine (radians) | `cos(0)` -> `1` |
| `tan(x)` | Tangent (radians) | `tan(0)` -> `0` |
| `log(x)` | Natural logarithm | `log(1)` -> `0` |
| `log2(x)` | Log base 2 | `log2(8)` -> `3` |
| `log10(x)` | Log base 10 | `log10(100)` -> `2` |

### List/Range Functions

| Function | Description |
|----------|-------------|
| `range(n)` | Generate list [0..n-1] |
| `random()` | Random float 0.0 to 1.0 |

### Assertion Functions

| Function | Description |
|----------|-------------|
| `assert(cond)` | Assert condition is true |
| `assert_eq(a, b)` | Assert equality |
| `assert_ne(a, b)` | Assert inequality |

### I/O Functions

| Function | Description |
|----------|-------------|
| `print(...)` | Print values separated by spaces |
| `input(prompt)` | Read user input |
| `exit()` | Exit program |

## Standard Library

### math.qs

```qscript
from "stdlib/math.qs" import min, max, clamp
print(min([3, 7, 1]))  # 1
print(max([3, 7, 1]))  # 7
print(clamp(5, 1, 3))  # 3
```

### text.qs

```qscript
from "stdlib/text.qs" import upper, lower, trim, split
print(upper("hello"))           # HELLO
print(lower("HELLO"))           # hello
print(trim("  hi  "))           # hi
parts = split("a,b,c", ",")
print(len(parts))               # 3
```

### json.qs

```qscript
from "stdlib/json.qs" import parse, stringify
data = {"name": "QS", "version": 1}
json_str = stringify(data)
parsed = parse(json_str)
print(parsed["name"])  # QS
```

### log.qs

```qscript
from "stdlib/log.qs" import info, warn, error
info("Application started")
warn("Low memory")
error("Something went wrong")
```

### fs.qs

```qscript
from "stdlib/fs.qs" import exists, read, write, remove
write("temp.txt", "hello world")
content = read("temp.txt")
print(content)  # hello world
remove("temp.txt")
```

### os.qs

**SECURITY WARNING:** `run()` and `capture()` execute shell commands directly. Do not pass untrusted user input without validation.

```qscript
from "stdlib/os.qs" import run, capture, cwd
rc = run("echo Hello")
text = capture("git --version")
dir = cwd()
```

**Note:** `os.exists()` checks if an executable command is in PATH, not whether a file exists. Use `fs.exists()` for file existence.

### time.qs

```qscript
from "stdlib/time.qs" import now, add_seconds, diff_seconds
t = now()
t2 = add_seconds(t, 3600)
print(diff_seconds(t2, t))  # 3600
```

**Note:** Functions like `year()`, `month()`, `today_str()`, `now_str()` internally call other functions in the same module. Importing just these functions will fail. Import all needed functions together.

### random.qs

```qscript
from "stdlib/random.qs" import seed, randint, choice
seed(42)
x = randint(1, 10)
items = [10, 20, 30]
c = choice(items)
```

### core.qs

```qscript
from "stdlib/core.qs" import env
path = env("PATH")
```

## Bytecode VM

The bytecode VM compiles QuantoScript to bytecode and executes it:

```bash
qs vm program.qs
```

### VM vs Interpreter

| Feature | Interpreter | VM |
|---------|-------------|-----|
| Arithmetic | Yes | Yes |
| Strings | Yes | Yes |
| Lists | Yes | Yes |
| Maps | Yes | Yes |
| Functions | Yes | Yes |
| Classes | Yes | Yes |
| Imports | Yes | Yes |
| Exceptions | Yes | Yes |
| Performance | Baseline | 2-3x faster |

The VM is recommended for production use.

### Bytecode Format

QuantoScript uses a stable `.qvm` binary format (version 4):

```bash
qs build program.qs                    # Compile to program.qvm
qs build program.qs -o custom.qvm     # Custom output name
qs vm program.qvm                      # Run compiled bytecode
qs vm --dump-bytecode program.qs       # Disassemble for debugging
```

## REPL

Interactive mode:

```bash
qs
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

- Multi-line block detection (auto-continues on `{`)
- Function definitions (`func`)
- Class definitions (`class`)
- Control flow (`if`/`maybe`/`else`, `while`, `repeat`, `try`/`oops`)
- Import support (`from ... import`)
- Assignment, print, and expression evaluation
- Exit via `exit` or `quit`

## CLI Commands

```bash
qs <file>                    # Run file (interpreter)
qs vm <file>                 # Run file (bytecode VM)
qs run <file>                # Run file (interpreter)
qs build <file>              # Compile to bytecode
qs build <file> -o out.qvm  # Compile to custom output
qs check <file>              # Syntax check
```

## Parser Limitations

These are known parser design constraints, not bugs:

1. **`||` and `&&` are not implemented**: Use `or` and `and` keywords.
2. **Method chaining on function return values fails**: `split(",").count` fails. Use intermediate variable.
3. **`break`/`continue` require multi-line block bodies**: Single-line `if` blocks inside loops may fail due to file loader line merging.

## Unsupported Features

These features are **not** implemented in QuantoScript v1.0.0:

- **Async/await** -- no native async support
- **Closures** with captured variables (inner functions return null)
- **Inheritance** -- no `extends`, `super`
- **Interfaces** -- no abstract methods
- **Generics** -- no type parameters
- **Pattern matching** -- no `match`/`switch`
- **Ternary operator** -- no `? :`
- **String interpolation** -- no `$"..."` syntax
- **List comprehensions** -- not implemented
- **Decorators** -- not implemented
- **Multiple return values** -- not implemented
- **Structs** -- not implemented
- **Enums** -- not implemented
- **Modules** -- imports are file-based, no module system
- **Default parameter values** -- not implemented
- **Rest parameters (`*args`)** -- not implemented
- **Named arguments (`**kwargs`)** -- not implemented
- **Return type enforcement** -- return types are parsed but not checked at runtime
- **Class field type enforcement** -- field types are parsed but not checked at runtime

## Implementation Notes

### Architecture

- **Amalgamation build** -- all `.inc` files are `#include`d into `quanto.c` for single-file compilation
- **Tree-walk interpreter** -- `executor.inc` evaluates AST nodes directly
- **Bytecode VM** -- `bc_compiler.inc` compiles to bytecode, `vm.inc` executes

### Performance

- VM runs 2-3x faster than the tree-walk interpreter
- String interning eliminates 70-90% of variable name comparisons

### Object Model

- Objects use shared identity (reference semantics) -- no deep copy
- `clone_value()` copies the pointer, not the object
- `c = a` means `c` and `a` point to the same object
