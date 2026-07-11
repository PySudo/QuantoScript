# New string methods

# contains
print("hello world".contains("world"))     # true
print("hello".contains("xyz"))             # false

# replace
print("hello world".replace("world", "QS"))  # hello QS
print("aabbcc".replace("bb", "xx"))          # aaxxcc

# startsWith
print("hello".startsWith("he"))           # true
print("hello".startsWith("lo"))           # false

# endsWith
print("hello".endsWith("lo"))             # true
print("hello".endsWith("he"))             # false

# title
print("hello world".title())              # Hello World
print("QUANTO script".title())            # Quanto Script
