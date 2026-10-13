[« Previous: Lambdas and Closures](10-lambdas-and-closures.md) · [Guide Index](README.md) · [Next: Error Handling »](12-error-handling.md)

# 11. Classes

A class bundles data (fields) with behaviour (methods). Calling the class
creates an *instance*, also called an object. Objects have **shared identity**:
two names can refer to the very same object.

---

## Defining a class

```qscript
class Person {
    init(name) {
        self.name = name
    }

    greet() {
        print(self.name)
    }
}

p = Person("Alice")
p.greet()               # Alice
```

- `init` is the constructor. It runs when the class is called.
- `self` refers to the current instance.
- Fields are created by assigning to `self.field`.

---

## Creating instances

Each call to the class produces a new, independent object:

```qscript
a = Person("Bob")
b = Person("Charlie")
a.greet()    # Bob
b.greet()    # Charlie
```

A class with no `init` can still be instantiated:

```qscript
class Empty {
}

e = Empty()
print("created")
```

---

## Methods

Methods are `func`-like but belong to the instance and receive `self`:

```qscript
class Counter {
    init() {
        self.x = 0
    }

    inc() {
        self.x = self.x + 1
    }

    get() {
        return self.x
    }
}

c = Counter()
c.inc()
c.inc()
print(c.x)      # 2
print(c.get())  # 2
```

Methods can call other methods and take arguments:

```qscript
class Math {
    add(x, y) {
        return x + y
    }

    double(x) {
        return x * 2
    }
}

m = Math()
print(m.add(3, 4))            # 7
print(m.double(m.add(1, 2)))  # 6
```

> Write **one statement per line** inside a multi-statement method body. This is
> required by the parser; see
> [Style and Limits](20-style-and-limits.md#known-limitations).

---

## Shared identity

Objects are references, not copies. Assigning an object to another name makes
both names point at the same instance:

```qscript
class Counter {
    init() {
        self.x = 0
    }

    inc() {
        self.x = self.x + 1
    }

    get() {
        return self.x
    }
}

a = Counter()
b = a          # b and a refer to the SAME object
b.inc()
print(a.get()) # 1
```

---

## Objects inside objects

Fields can hold other objects, and you can chain field access:

```qscript
class Inner {
    init(v) {
        self.v = v
    }
}

class Outer {
    init() {
        self.inner = Inner(77)
    }
}

o = Outer()
print(o.inner.v)   # 77
```

---

## Next steps

[Error Handling](12-error-handling.md) covers `try` and `oops`.

---

[« Previous: Lambdas and Closures](10-lambdas-and-closures.md) · [Guide Index](README.md) · [Next: Error Handling »](12-error-handling.md)
