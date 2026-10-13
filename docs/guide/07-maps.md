[« Previous: Lists](06-lists.md) · [Guide Index](README.md) · [Next: Control Flow »](08-control-flow.md)

# 7. Maps

A map (also called a dictionary) stores values under keys. Keys are strings;
values can be anything.

---

## Creating maps

Two forms are available — a literal with `:` and a constructor with `=`:

```qscript
user = {"name": "Sara", "age": 20}

person = map("name" = "Sara", "age" = 20, "city" = "Tehran")
```

The `map(...)` form is handy when keys are simple names.

---

## Reading and writing

Use square brackets to read a value by key, and the same syntax to create or
update one:

```qscript
user = {"name": "Sara"}

print(user["name"])     # Sara

user["age"] = 20        # add a new key
user["name"] = "Mina"   # update an existing key

print(user["age"])      # 20
print(user)             # {'name': 'Mina', 'age': 20}
```

---

## Map methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `has(key)` | test whether a key exists | `map("a"=1).has("a")` | `true` |
| `get(key, default)` | value for `key`, or `default` if missing | `map("a"=1).get("b", 0)` | `0` |
| `keys()` | list of keys | `map("a"=1).keys()` | `['a']` |
| `values()` | list of values | `map("a"=1).values()` | `[1]` |
| `items()` | list of `[key, value]` pairs | `map("a"=1).items()` | `[['a', 1]]` |
| `remove(key)` | delete a key | `map("a"=1).remove("a")` | — |
| `extend(other)` | merge another map in | `map("a"=1).extend(map("b"=2))` | `{'a': 1, 'b': 2}` |

```qscript
person = map("name" = "Sara", "age" = 20, "city" = "Tehran")

print(person.has("name"))     # true
print(person.has("email"))    # false

print(person.get("name", "?"))      # Sara
print(person.get("email", "none"))  # none  (missing key -> default)

print(person.keys())          # ['name', 'age', 'city']
print(person.values())        # ['Sara', 20, 'Tehran']
print(person.items())         # [['name', 'Sara'], ['age', 20], ['city', 'Tehran']]

person.remove("city")
print(person.keys())          # ['name', 'age']

person.extend(map("country" = "Iran"))
print(person.keys())          # ['name', 'age', 'country']
```

Because maps have no direct `len`, count their keys instead:

```qscript
print(len(person.keys()))
```

---

## Equality

Maps compare by value, regardless of insertion order:

```qscript
print(map("a"=1, "b"=2) == map("b"=2, "a"=1))   # true
```

---

## Iterating

Looping over a map visits its keys:

```qscript
person = map("name" = "Sara", "age" = 20)

repeat key -> person {
    print(key, person[key])
}
```

See [Control Flow](08-control-flow.md) for more iteration forms.

---

## Next steps

[Control Flow](08-control-flow.md) covers conditions and loops.

---

[« Previous: Lists](06-lists.md) · [Guide Index](README.md) · [Next: Control Flow »](08-control-flow.md)
