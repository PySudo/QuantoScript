[« Previous: Control Flow](08-control-flow.md) · [Guide Index](README.md) · [Next: Lambdas and Closures »](10-lambdas-and-closures.md)

# 9. Functions

Functions group statements behind a name so you can reuse them. They are
introduced with the `func` keyword.

---

## Defining and calling

```qscript
func greet(name) {
    print("hello", name)
}

greet("Ali")   # hello Ali
```

A function returns a value with `return`:

```qscript
func add(a, b) {
    return a + b
}

print(add(3, 4))   # 7
```

A function that reaches its end without a `return` produces `null`.

---

## Keyword arguments

An argument may be named at the call site with `name = value`. Named arguments
can appear in any order and may be mixed with positional ones:

```qscript
func box(w, h) {
    return w * h
}

print(box(h = 3, w = 4))   # 12
print(box(4, h = 3))       # 12
```

The names must match the function's parameters. A function that collects extra
named arguments with `**kwargs` receives any names it does not declare (see
[Variadic parameters](#variadic-parameters)).

---

## Type annotations

A parameter may be annotated with a type. The annotation documents intent and is
enforced when the function is called:

```qscript
func add(a:int, b:int) {
    return a + b
}

print(add(2, 3))     # 5
# add("x", 1)        # error: wrong argument type
```

The available type names match `type()`: `int`, `float`, `string`, `bool`,
`null`, `list`, `map`.

Omitting the annotation accepts any value:

```qscript
func show(value) {
    print("value:", value)
}

show("text")
show(42)
```

---

## Default parameter values

A parameter can declare a default, used when the argument is omitted:

```qscript
func say(text:string, ending:string = " world") {
    return text + ending
}

print(say("hello"))            # hello world
print(say("hello", " Quanto")) # hello Quanto
```

---

## Union types

A parameter can accept more than one type using `|`. Use `isinstance` to branch
on the actual type:

```qscript
func greet_any(name:string|int) {
    if isinstance(name, "int") {
        print("hello number", name)
    } else {
        print("hello", name)
    }
}

greet_any("Sara")   # hello Sara
greet_any(42)       # hello number 42
```

Unions can list several types:

```qscript
func show(value:string|int|bool) {
    print("value:", value)
}
```

---

## Variadic parameters

### `*args` — extra positional arguments

A parameter written `*rest` collects all remaining positional arguments into a
list:

```qscript
func collect(first, *rest) {
    print("first:", first)
    print("rest:", rest)
}

collect("a", "b", "c", "d")
# first: a
# rest: ['b', 'c', 'd']
```

### `**kwargs` — extra keyword arguments

A parameter written `**info` collects all remaining keyword arguments into a map:

```qscript
func profile(name:string, **info) {
    print("name:", name)
    print("age:", info["age"])
    print("city:", info["city"])
}

profile("Sara", age=20, city="Tehran")
```

---

## Recursion

Functions may call themselves:

```qscript
func factorial(n) {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

print(factorial(5))   # 120
```

---

## Does a function ever return a value?

`ever_return(fn)` inspects a function and reports whether it can produce a value.
A function that only prints, or that never finishes (for example one that calls
`exit`), reports `false`:

```qscript
func gives_number() {
    return 2
}

func gives_null() {
}

func stops() {
    exit()
}

print(ever_return(gives_number))   # true
print(ever_return(gives_null))     # false
print(ever_return(stops))          # false
```

---

## Functions are values

Because functions are values, they can be passed to other functions and stored
in variables — this is exactly what `map`, `filter`, and `find` rely on. See
[Lambdas and Closures](10-lambdas-and-closures.md).

---

## Next steps

[Lambdas and Closures](10-lambdas-and-closures.md) covers anonymous functions
and environment capture.

---

[« Previous: Control Flow](08-control-flow.md) · [Guide Index](README.md) · [Next: Lambdas and Closures »](10-lambdas-and-closures.md)
