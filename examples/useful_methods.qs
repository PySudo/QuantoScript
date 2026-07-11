# Useful methods and functions

# Lists: append adds one item.
numbers = [3, 1, 2]
numbers.append(4)
print("after append:", numbers)

# Lists: extend adds many items from another list.
numbers.extend([6, 5])
print("after extend:", numbers)

# Lists: sort puts items in order.
numbers.sort()
print("after sort:", numbers)

# Methods can also return a new value inside an expression.
more = [2, 1].append(3).sort()
print("method chain:", more)

# Strings: split breaks a string into a list.
words = "hello world foo".split(" ")
print("split:", words)

csv = "red,green,blue".split(",")
print("csv split:", csv)

chars = "abc".split("")
print("char split:", chars)

# Strings: join glues list items together.
names = ["Ali", "Sara", "Mina"]
line = ", ".join(names)
print("joined:", line)

# Numbers: abs returns the positive distance from zero.
print("abs:", abs(-12))

# len works on strings and lists.
print("name count:", len(names))
print("letter count:", len("Quanto"))

# type() returns the type name as a string.
print("type of 42:", type(42))
print("type of hi:", type("hi"))
print("type of true:", type(true))
print("type of null:", type(null))
print("type of list:", type([1, 2]))
print("type of map:", type({"a": 1}))

# str() converts a value to a string.
print("str(42):", str(42))
print("str([1,2]):", str([1, 2]))
