[« Previous: Strings](05-strings.md) · [Guide Index](README.md) · [Next: Maps »](07-maps.md)

# 6. Lists

A list is an ordered collection of values. Like strings, lists use **1-based**
indexing and support negative indexes and slices.

---

## Creating lists

```qscript
items = ["red", "green", "blue"]
mixed = ["hello", 10, true, null, ["nested", 2 + 3]]

print(len(items))   # 3
```

Lists may contain any mix of values, including other lists.

---

## Indexing and slicing

```qscript
items = ["red", "green", "blue"]

print(items[1])      # red      (first)
print(items[-1])     # blue     (last)
print(items[:2])     # ['red', 'green']
print(items[-2:])    # ['green', 'blue']
```

Nested lists are indexed with repeated brackets:

```qscript
mixed = ["hello", 10, true, null, ["nested", 5]]
print(mixed[5][1])   # nested
```

---

## Ranges

`range` produces a list of numbers, which is convenient for loops:

```qscript
print(range(5))         # [1, 2, 3, 4, 5]
print(range(2, 5))      # [2, 3, 4, 5]
print(range(5, 1, -2))  # [5, 3, 1]
```

---

## List methods

| Method | Description | Example | Result |
|--------|-------------|---------|--------|
| `append(x)` | add one item to the end | `[1, 2].append(3)` | `[1, 2, 3]` |
| `extend(list)` | add all items from another list | `[1].extend([2, 3])` | `[1, 2, 3]` |
| `sort()` | sort items in place | `[3, 1, 2].sort()` | `[1, 2, 3]` |
| `contains(x)` | test for an element | `[1, 2, 3].contains(2)` | `true` |
| `flatten()` | flatten one level of nesting | `[[1, 2], [3]].flatten()` | `[1, 2, 3]` |
| `sum()` | add all numbers | `[1, 2, 3].sum()` | `6` |
| `map(fn)` | transform each element | `[1, 2].map(fn(x) -> x * 2)` | `[2, 4]` |
| `filter(fn)` | keep elements where `fn` is true | `[1, 2, 3].filter(fn(x) -> x > 1)` | `[2, 3]` |
| `find(x)` | first element equal to `x` | `[1, 2, 3].find(2)` | `2` |
| `find(fn)` | first element where `fn` is true | `[1, 2, 3].find(fn(x) -> x > 1)` | `2` |
| `find_type(t)` | first element of a type | `[1, "hi"].find_type("string")` | `hi` |

```qscript
numbers = [3, 1, 2]
numbers.append(4)          # [3, 1, 2, 4]
numbers.extend([6, 5])     # [3, 1, 2, 4, 6, 5]
numbers.sort()             # [1, 2, 3, 4, 5, 6]

print(numbers.contains(3))          # true
print(numbers.sum())                # 21

print([[1, 2], [3, 4], [5]].flatten())   # [1, 2, 3, 4, 5]

print([1, 2, 3].map(fn(x) -> x * 2))     # [2, 4, 6]
print([1, 2, 3, 4, 5].filter(fn(x) -> x % 2 == 0))   # [2, 4]
print([1, 2, 3, 4, 5].find(fn(x) -> x > 3))          # 4
print([1, "hello", 2].find_type("string"))           # hello
```

---

## Chaining

Methods that return the list can be chained:

```qscript
result = [5, 3, 1].sort().append(0)
print(result)   # [1, 3, 5, 0]

more = [2, 1].append(3).sort()
print(more)     # [1, 2, 3]
```

---

## Equality and length

Lists compare by value, recursively:

```qscript
print([1, 2] == [1, 2])        # true
print([1, 2] == [1, 3])        # false
print([1, [2, 3]] == [1, [2, 3]])   # true

print(len([10, 20, 30]))       # 3
```

---

## Iterating

```qscript
names = ["Ali", "Sara", "Mina"]

repeat name -> names {
    print(name)
}

repeat i, name -> names {
    print(i, name)
}
```

See [Control Flow](08-control-flow.md) for every iteration form.

---

## Next steps

Maps are the subject of [Maps](07-maps.md).

---

[« Previous: Strings](05-strings.md) · [Guide Index](README.md) · [Next: Maps »](07-maps.md)
