# QuantoScript Classes - v0.1.0
# Basic object-oriented programming support

# ─── Basic class with constructor ───

class Person {
    init(name) {
        self.name = name
    }

    greet() {
        print(self.name)
    }
}

p = Person("Alice")
p.greet()                    # Alice

# ─── Multiple objects ───

a = Person("Bob")
b = Person("Charlie")
a.greet()                    # Bob
b.greet()                    # Charlie

# ─── Class with multiple fields (must use multi-line bodies) ───

class Point {
    init(x) {
        self.x = x
        self.y = 0
    }
}

pt = Point(3)
print(pt.x)                  # 3

# ─── Object without constructor ───

class Empty {
}

e = Empty()
print("created")             # created

# ─── Counter example ───

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
print(c.x)                   # 2

# ─── Object references (shared identity) ───

a = Counter()
b = a
b.inc()
print(a.get())               # 1 (shared identity)

# ─── Nested objects ───

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
print(o.inner.v)             # 77

# ─── Multiple method calls ───

class Math {
    add(x, y) {
        return x + y
    }

    double(x) {
        return x * 2
    }
}

m = Math()
print(m.add(3, 4))           # 7
print(m.double(m.add(1, 2))) # 6
