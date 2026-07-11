# Lambda functions with fn(params) -> body

# Basic lambda
double = fn(x) -> x * 2
print(double(5))   # 10

# Multiple params
add = fn(a, b) -> a + b
print(add(2, 3))   # 5

# Type hints
typed_add = fn(x:int, y:int) -> x + y
print(typed_add(10, 20))   # 30

# Union types
concat = fn(x:int|string, y:int|string) -> x + y
print(concat('hello ', 2))        # hello 2
print(concat('hello', ' world'))  # hello world

# Currying
add_all = fn(x, y) -> fn(z) -> x + y + z
one = add_all(2, 5)
two = one(3)
print(two)   # 10

# Closures capture environment
n = 10
add_n = fn(x) -> x + n
print(add_n(5))   # 15

# Use with list methods
numbers = [1, 2, 3, 4, 5]
doubled = numbers.map(fn(x) -> x * 2)
print(doubled)     # [2, 4, 6, 8, 10]

evens = numbers.filter(fn(x) -> x % 2 == 0)
print(evens)       # [2, 4]

found = numbers.find(fn(x) -> x > 3)
print(found)       # 4
