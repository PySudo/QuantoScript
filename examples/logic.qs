# Logic

age = 7

print(age > 4)
print(age > 4 and age < 10)
print(age < 4 or age == 7)
print(not false)

# "in" checks if something exists inside a string or list.
print("a" in "cat")
print("red" in ["red", "blue"])
print(4 in [1, 2, 3])

# Chained comparisons work like Python.
x = 3
print("0 < 3 < 5:", 0 < x < 5)
print("0 < 3 < 2:", 0 < x < 2)

# Truthy and falsy
print("not null:", not null)
print("not false:", not false)
print("not 0:", not 0)
print("not empty string:", not "")
print("not empty list:", not [])

# Only null, false, 0, empty string, and empty list are falsy.
# Everything else is truthy.
print("10 == true:", 10 == true)
print("0 == true:", 0 == true)
print("non-empty string is truthy:", not ("hello" == false))
