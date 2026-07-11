# Lists
# Lists can hold numbers, strings, booleans, null, expressions, and other lists.

items = ["red", "green", "blue"]
mixed = ["hello", 10, true, null, ["nested", 2 + 3]]

print(items)
print(mixed)

# Indexes start at 1.
print("first:", items[1])
print("last:", items[-1])

# Nested indexing works too.
print("nested value:", mixed[5][1])

# Slices work on lists.
print("first three:", items[:3])
print("last two:", items[-2:])

# append adds one item to the end.
numbers = [3, 1, 2]
numbers.append(4)
print("after append:", numbers)

# extend adds many items from another list.
numbers.extend([6, 5])
print("after extend:", numbers)

# sort puts items in order.
numbers.sort()
print("after sort:", numbers)

# contains checks if a value exists in the list.
print("contains 3:", numbers.contains(3))
print("contains 99:", numbers.contains(99))

# flatten turns nested lists into one flat list.
nested = [[1, 2], [3, 4], [5]]
print("flatten:", nested.flatten())

# sum adds up all numbers in the list.
print("sum:", numbers.sum())

# map transforms each element with a function.
doubled = [1, 2, 3].map(fn(x) -> x * 2)
print("map double:", doubled)

as_strings = [1, 2, 3].map(fn(x) -> str(x))
print("map to string:", as_strings)

# filter keeps elements where the function returns true.
evens = [1, 2, 3, 4, 5].filter(fn(x) -> x % 2 == 0)
print("filter evens:", evens)

ints_only = [1, "hi", 2, "world"].filter(fn(x) -> isinstance(x, "int"))
print("filter ints:", ints_only)

# find returns the first element matching a value.
found = [1, 2, 3, 4, 5].find(3)
print("find 3:", found)

# find with a lambda returns the first element where the function is true.
first_big = [1, 2, 3, 4, 5].find(fn(x) -> x > 3)
print("first > 3:", first_big)

# find_type returns the first element of a given type.
first_str = [1, "hello", 2].find_type("string")
print("first string:", first_str)

# Method chaining works.
result = [5, 3, 1].sort().append(0)
print("chained:", result)

# Equality compares lists deeply.
print("list eq:", [1, 2] == [1, 2])
print("list ne:", [1, 2] == [1, 3])

# len() returns the number of items.
print("length:", len([10, 20, 30]))
